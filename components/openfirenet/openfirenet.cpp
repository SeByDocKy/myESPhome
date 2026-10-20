#include "openfirenet.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif

namespace esphome {
namespace openfirenet {

static const char *const TAG = "openfirenet";

static const char *const MODE_NAMES[] = {"Manual", "Auto", "Comfort", "Setback"};
static constexpr size_t MODE_COUNT = 4;

// Number of sensor slots requested from the stove: highest slot we decode (54) + 1.
static constexpr size_t SLOTS_REQUESTED = SLOT_ON_OFF_CYCLES + 1;

static inline bool time_reached(uint32_t now, uint32_t target) { return static_cast<int32_t>(now - target) >= 0; }

void OpenFireNetComponent::setup() {
  this->controls_.assign(NUM_CONTROLS, 0);
  this->pending_controls_.assign(NUM_CONTROLS, 0);
  this->rx_buf_.reserve(2048);
  this->last_stove_activity_ms_ = millis();
  // Chunked TX with a few ms between chunks needs a loop faster than the default 16 ms.
  this->high_freq_.start();
}

void OpenFireNetComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenFireNet (RIKA Firenet 2.0 USB dongle emulation):");
  ESP_LOGCONFIG(TAG, "  Frame gap: %u ms", static_cast<unsigned>(this->frame_gap_ms_));
  LOG_UPDATE_INTERVAL(this);
}

// ---------------------------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------------------------

void OpenFireNetComponent::loop() {
  this->rx_poll_();
  this->tx_poll_();
  if (this->handshake_ != Handshake::WAITING &&
      time_reached(millis(), this->last_stove_activity_ms_ + LINK_TIMEOUT_MS)) {
    ESP_LOGW(TAG, "No traffic from the stove for %u s, link considered lost",
             static_cast<unsigned>(LINK_TIMEOUT_MS / 1000));
    this->handshake_ = Handshake::WAITING;
    this->tx_queue_.clear();
    this->tx_current_.clear();
    this->controls_valid_ = false;
    this->slots_valid_ = false;
    this->publish_connected_(false);
  }
}

void OpenFireNetComponent::update() {
  if (this->handshake_ != Handshake::READY)
    return;
  if (!this->tx_queue_.empty() || !this->tx_current_.empty())
    return;  // previous cycle still going out
  this->enqueue_cycle_();
}

// ---------------------------------------------------------------------------------------------
// RX
// ---------------------------------------------------------------------------------------------

void OpenFireNetComponent::rx_poll_() {
  const uint32_t now = millis();
  size_t avail = this->available();
  if (avail > 0) {
    uint8_t tmp[64];
    while (avail > 0) {
      const size_t n = std::min(avail, sizeof(tmp));
      if (!this->read_array(tmp, n))
        break;
      this->rx_buf_.append(reinterpret_cast<const char *>(tmp), n);
      avail -= n;
      if (this->rx_buf_.size() >= RX_MAX_FRAME)
        break;
    }
    this->last_rx_ms_ = now;
  }
  if (this->rx_buf_.empty())
    return;
  // A frame ends with a short silence on the line.
  if (this->rx_buf_.size() >= RX_MAX_FRAME || time_reached(now, this->last_rx_ms_ + RX_SILENCE_MS)) {
    std::string frame;
    frame.swap(this->rx_buf_);
    this->process_frame_(frame);
  }
}

void OpenFireNetComponent::process_frame_(const std::string &frame) {
  ESP_LOGV(TAG, "RX %u bytes: %s", static_cast<unsigned>(frame.size()), printable(frame).c_str());
  if (static_cast<uint8_t>(frame[0]) == PROBE_SYN) {
    this->on_probe_();
    return;
  }
  this->last_stove_activity_ms_ = millis();
  std::vector<Command> cmds;
  split_commands(frame, cmds);
  if (cmds.empty()) {
    ESP_LOGD(TAG, "Unrecognised frame: %s", printable(frame).c_str());
    return;
  }
  for (const auto &cmd : cmds)
    this->handle_command_(cmd);
}

void OpenFireNetComponent::handle_command_(const Command &cmd) {
  if (cmd.name == "GET_CDCDEVICE_VERSION_FINISHED") {
    this->on_version_finished_();
  } else if (cmd.name == "POST_CDCDEVICE_STATUS") {
    this->on_stove_status_(cmd.text);
  } else if (cmd.name == "POST_CONTROLS") {
    this->on_controls_(cmd.text);
  } else if (cmd.name == "POST_SENSORS") {
    this->on_sensors_(cmd.text);
  } else {
    ESP_LOGD(TAG, "Ignoring command %s", cmd.name.c_str());
  }
}

void OpenFireNetComponent::on_probe_() {
  const uint32_t now = millis();
  // The stove repeats its probe every ~100 ms until it gets the version frame: do not answer
  // each one while we are already waiting for the handshake to finish.
  if (this->handshake_ == Handshake::VERSION_SENT && !time_reached(now, this->last_version_ms_ + 1500))
    return;
  if (this->handshake_ == Handshake::READY)
    ESP_LOGW(TAG, "Stove restarted its handshake, resynchronising");
  else
    ESP_LOGD(TAG, "Stove probe received, sending version frame");
  this->tx_queue_.clear();
  this->tx_current_.clear();
  this->tx_offset_ = 0;
  this->controls_valid_ = false;
  this->handshake_ = Handshake::VERSION_SENT;
  this->last_version_ms_ = now;
  this->last_stove_activity_ms_ = now;
  this->enqueue_(build_version_frame());
}

void OpenFireNetComponent::on_version_finished_() {
  ESP_LOGI(TAG, "Handshake complete");
  this->handshake_ = Handshake::READY;
  this->publish_connected_(true);
  // Start polling right away instead of waiting for the next update_interval.
  if (this->tx_queue_.empty() && this->tx_current_.empty())
    this->enqueue_cycle_();
}

void OpenFireNetComponent::on_stove_status_(const std::string &segment) {
  const StoveStatus st = parse_stove_status(segment);
  if (!st.valid) {
    ESP_LOGW(TAG, "Malformed POST_CDCDEVICE_STATUS: %s", printable(segment).c_str());
    return;
  }
  this->status_.id = st.id;
  this->status_.token = st.token;
  this->status_.ssid_hex = st.ssid_hex;
  this->status_.wpa2 = st.wpa2;
  this->status_seen_ = true;
#ifdef USE_WIFI
  this->status_.wifi_connected = wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected();
  if (this->status_.wifi_connected) {
    this->status_.rssi = wifi::global_wifi_component->wifi_rssi();
    char mac[MAC_ADDRESS_BUFFER_SIZE];
    get_mac_address_into_buffer(mac);
    this->status_.mac = mac;
  }
#endif
#ifdef USE_NETWORK
  {
    auto ips = network::get_ip_addresses();
    for (auto &ip : ips) {
      char buf[network::IP_ADDRESS_BUFFER_SIZE];
      ip.str_to(buf);
      // First usable IPv4 address (skip unset / IPv6 entries).
      if (buf[0] != '\0' && std::strcmp(buf, "0.0.0.0") != 0 && std::strchr(buf, ':') == nullptr) {
        this->status_.ip = buf;
        break;
      }
    }
  }
#endif
  ESP_LOGD(TAG, "Stove status received (scan_command=%ld), answering", st.scan_command);
  this->enqueue_(build_status_frame(this->status_));
}

void OpenFireNetComponent::on_controls_(const std::string &segment) {
  std::vector<long> v;
  parse_positional_values(segment, v);
  if (v.size() < NUM_CONTROLS) {
    ESP_LOGW(TAG, "POST_CONTROLS with %u values, expected %u", static_cast<unsigned>(v.size()),
             static_cast<unsigned>(NUM_CONTROLS));
    return;
  }
  v.resize(NUM_CONTROLS);
  this->controls_ = v;
  this->controls_valid_ = true;
  if (this->hold_active_) {
    if (v == this->pending_controls_ || time_reached(millis(), this->write_hold_until_ms_)) {
      this->hold_active_ = false;
    } else if (this->write_dirty_) {
      // Our write has not even been sent yet: keep the pending view.
    } else {
      // Echo of the old values: the stove has not applied our write yet. Keep the optimistic view.
      ESP_LOGV(TAG, "Stove still reports previous controls, holding pending values");
    }
  }
  this->publish_controls_();
}

void OpenFireNetComponent::on_sensors_(const std::string &segment) {
  parse_positional_values(segment, this->slots_);
  this->slots_valid_ = !this->slots_.empty();
  if (this->slots_.size() < SLOTS_REQUESTED)
    ESP_LOGD(TAG, "POST_SENSORS carries %u slots (%u requested)", static_cast<unsigned>(this->slots_.size()),
             static_cast<unsigned>(SLOTS_REQUESTED));
  this->publish_sensors_();
}

// ---------------------------------------------------------------------------------------------
// TX
// ---------------------------------------------------------------------------------------------

void OpenFireNetComponent::enqueue_(std::string frame) {
  if (this->tx_queue_.size() >= MAX_TX_QUEUE) {
    ESP_LOGW(TAG, "TX queue full, dropping oldest frame");
    this->tx_queue_.pop_front();
  }
  this->tx_queue_.push_back(std::move(frame));
}

void OpenFireNetComponent::enqueue_cycle_() {
  const uint32_t now = millis();
  if (this->write_dirty_ && this->controls_valid_) {
    this->enqueue_(build_controls_write(this->pending_controls_));
    this->write_dirty_ = false;
    this->write_hold_until_ms_ = now + WRITE_HOLD_MS;
  }
  this->enqueue_(build_controls_read());
  this->enqueue_(build_sensors_read(SLOTS_REQUESTED));
  int rssi = 0;
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected())
    rssi = wifi::global_wifi_component->wifi_rssi();
#endif
  this->enqueue_(build_revision_frame(rssi, this->controls_valid_ ? this->controls_[CTRL_REVISION] : 0));
  for (size_t i = 0; i < TRANSFER_COMPLETED_PER_CYCLE; i++)
    this->enqueue_(build_transfer_completed());
}

void OpenFireNetComponent::tx_poll_() {
  const uint32_t now = millis();
  if (!time_reached(now, this->tx_next_ms_))
    return;
  if (this->tx_current_.empty()) {
    if (this->tx_queue_.empty())
      return;
    this->tx_current_ = std::move(this->tx_queue_.front());
    this->tx_queue_.pop_front();
    this->tx_offset_ = 0;
    ESP_LOGV(TAG, "TX frame %u bytes: %s", static_cast<unsigned>(this->tx_current_.size()),
             printable(this->tx_current_).c_str());
  }
  const size_t remaining = this->tx_current_.size() - this->tx_offset_;
  const size_t n = is_version_frame(this->tx_current_) ? remaining : std::min(remaining, TX_CHUNK_SIZE);
  this->write_array(reinterpret_cast<const uint8_t *>(this->tx_current_.data()) + this->tx_offset_, n);
  // Flush after every chunk so that each chunk leaves as its own (short) USB packet.
  if (this->flush() == uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT)
    ESP_LOGW(TAG, "USB flush timed out (is the stove reading the port?)");
  this->tx_offset_ += n;
  if (this->tx_offset_ >= this->tx_current_.size()) {
    this->tx_current_.clear();
    this->tx_offset_ = 0;
    this->tx_next_ms_ = now + this->frame_gap_ms_;
  } else {
    this->tx_next_ms_ = now + CHUNK_SPACING_MS;
  }
}

// ---------------------------------------------------------------------------------------------
// Control API
// ---------------------------------------------------------------------------------------------

bool OpenFireNetComponent::write_control_(ControlField field, long value) {
  if (this->handshake_ != Handshake::READY || !this->controls_valid_) {
    ESP_LOGW(TAG, "Cannot write control now: stove link not ready or controls not read yet");
    return false;
  }
  if (!this->hold_active_)
    this->pending_controls_ = this->controls_;
  this->pending_controls_[field] = value;
  this->hold_active_ = true;
  this->write_dirty_ = true;
  this->write_hold_until_ms_ = millis() + WRITE_HOLD_MS;
  this->publish_controls_();
  if (this->tx_queue_.empty() && this->tx_current_.empty())
    this->enqueue_cycle_();
  return true;
}

bool OpenFireNetComponent::set_power(bool on) { return this->write_control_(CTRL_ON_OFF, on ? 1 : 0); }

bool OpenFireNetComponent::set_target_temperature(float celsius) {
  long v = lroundf(celsius * 10.0f);
  v = std::max(140L, std::min(280L, v));
  return this->write_control_(CTRL_ROOM_TARGET, v);
}

bool OpenFireNetComponent::set_heating_power(float percent) {
  long v = lroundf(percent);
  v = std::max(30L, std::min(100L, v));
  return this->write_control_(CTRL_TARGET_STAGE, v);
}

bool OpenFireNetComponent::set_mode(long mode) {
  if (mode < 0 || mode >= static_cast<long>(MODE_COUNT))
    return false;
  return this->write_control_(CTRL_MODE, mode);
}

// ---------------------------------------------------------------------------------------------
// Publishing
// ---------------------------------------------------------------------------------------------

void OpenFireNetComponent::publish_connected_(bool connected) {
  this->connected_ = connected;
#ifdef USE_BINARY_SENSOR
  this->publish_binary_(BINARY_SENSOR_CONNECTED, connected);
#endif
}

#ifdef USE_SENSOR
void OpenFireNetComponent::publish_sensor_(SensorKey key, float value) {
  if (this->sensors_[key] != nullptr)
    this->sensors_[key]->publish_state(value);
}
#endif
#ifdef USE_TEXT_SENSOR
void OpenFireNetComponent::publish_text_(TextSensorKey key, const std::string &value) {
  if (this->text_sensors_[key] == nullptr || this->text_cache_[key] == value)
    return;
  this->text_cache_[key] = value;
  this->text_sensors_[key]->publish_state(value);
}
#endif
#ifdef USE_BINARY_SENSOR
void OpenFireNetComponent::publish_binary_(BinarySensorKey key, bool value) {
  if (this->binary_sensors_[key] != nullptr)
    this->binary_sensors_[key]->publish_state(value);
}
#endif

void OpenFireNetComponent::publish_controls_() {
  if (!this->controls_valid_)
    return;
  const std::vector<long> &c = this->hold_active_ ? this->pending_controls_ : this->controls_;
#ifdef USE_SENSOR
  this->publish_sensor_(SENSOR_TARGET_TEMPERATURE, c[CTRL_ROOM_TARGET] / 10.0f);
  this->publish_sensor_(SENSOR_HEATING_POWER, static_cast<float>(c[CTRL_TARGET_STAGE]));
#endif
#ifdef USE_SWITCH
  if (this->power_switch_ != nullptr)
    this->power_switch_->publish_state(c[CTRL_ON_OFF] != 0);
#endif
#ifdef USE_NUMBER
  if (this->target_temperature_number_ != nullptr)
    this->target_temperature_number_->publish_state(c[CTRL_ROOM_TARGET] / 10.0f);
  if (this->heating_power_number_ != nullptr)
    this->heating_power_number_->publish_state(static_cast<float>(c[CTRL_TARGET_STAGE]));
#endif
  const long mode = c[CTRL_MODE];
#ifdef USE_SELECT
  if (this->mode_select_ != nullptr && mode >= 0 && mode < static_cast<long>(MODE_COUNT))
    this->mode_select_->publish_state(static_cast<size_t>(mode));
#endif
#ifdef USE_TEXT_SENSOR
  this->publish_text_(TEXT_SENSOR_MODE,
                      (mode >= 0 && mode < static_cast<long>(MODE_COUNT)) ? MODE_NAMES[mode] : fmt("Unknown (%ld)", mode));
#endif
}

void OpenFireNetComponent::publish_sensors_() {
  if (!this->slots_valid_)
    return;
  auto slot = [this](size_t i, long &out) -> bool {
    if (i >= this->slots_.size())
      return false;
    out = this->slots_[i];
    return true;
  };
  long v = 0, v2 = 0;
#ifdef USE_SENSOR
  // Temperatures are sent multiplied by 10 on the wire.
  if (slot(SLOT_ROOM_TEMP, v))
    this->publish_sensor_(SENSOR_ROOM_TEMPERATURE, v / 10.0f);
  if (slot(SLOT_FLAME, v))
    this->publish_sensor_(SENSOR_FLAME_TEMPERATURE, static_cast<float>(v));
  if (slot(SLOT_ID_FAN_MEAS, v))
    this->publish_sensor_(SENSOR_ID_FAN_MEASURED, static_cast<float>(v));
  if (slot(SLOT_ID_FAN_SET, v))
    this->publish_sensor_(SENSOR_ID_FAN_SET, static_cast<float>(v));
  if (slot(SLOT_BOARD_SENSOR, v))
    this->publish_sensor_(SENSOR_BOARD_TEMPERATURE, v / 10.0f);  // scale unverified on hardware
  if (slot(SLOT_RSSI, v))
    this->publish_sensor_(SENSOR_RSSI, static_cast<float>(v));
  if (slot(SLOT_PELLET_HOURS, v))
    this->publish_sensor_(SENSOR_PELLET_HOURS, static_cast<float>(v));
  if (slot(SLOT_PELLETS_TOTAL, v))
    this->publish_sensor_(SENSOR_PELLETS_TOTAL, static_cast<float>(v));
  if (slot(SLOT_SERVICE_COUNTDOWN, v))
    this->publish_sensor_(SENSOR_SERVICE_COUNTDOWN, static_cast<float>(v));
  if (slot(SLOT_IGNITION_COUNT, v))
    this->publish_sensor_(SENSOR_IGNITION_COUNT, static_cast<float>(v));
  if (slot(SLOT_ON_OFF_CYCLES, v))
    this->publish_sensor_(SENSOR_ON_OFF_CYCLES, static_cast<float>(v));
  if (slot(SLOT_ERR_MASK, v))
    this->publish_sensor_(SENSOR_ERROR_CODE, static_cast<float>(v));
#endif
#ifdef USE_TEXT_SENSOR
  if (slot(SLOT_MAIN_STATE, v)) {
    if (!slot(SLOT_SUB_STATE, v2))
      v2 = 0;
    this->publish_text_(TEXT_SENSOR_STATE, main_state_name(v, v2));
  }
  if (slot(SLOT_ERR_MASK, v))
    this->publish_text_(TEXT_SENSOR_ERROR, error_text(v));
  if (slot(SLOT_MODEL, v)) {
    if (!slot(SLOT_FAB_NUMBER, v2))
      v2 = 1;
    this->publish_text_(TEXT_SENSOR_MODEL, model_name(v, v2));
  }
  if (slot(SLOT_APP_VER_BOARD, v) && slot(SLOT_FIRMWARE_BUILD, v2))
    this->publish_text_(TEXT_SENSOR_FIRMWARE, firmware_text(v, v2));
#endif
#ifdef USE_BINARY_SENSOR
  if (slot(SLOT_MAIN_STATE, v))
    this->publish_binary_(BINARY_SENSOR_BURNING, main_state_burning(v));
  if (slot(SLOT_ERR_MASK, v))
    this->publish_binary_(BINARY_SENSOR_ERROR, v != 0);
  if (slot(SLOT_HOPPER_LID_CLOSED, v))
    this->publish_binary_(BINARY_SENSOR_HOPPER_LID_OPEN, v == 0);
#endif
}

}  // namespace openfirenet
}  // namespace esphome
