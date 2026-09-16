#include "ez1m.h"
#ifdef USE_SENSOR
#include "sensor/ez1m_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "text_sensor/ez1m_text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "number/ez1m_number.h"
#endif
#ifdef USE_SWITCH
#include "switch/ez1m_switch.h"
#endif
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cmath>
#include <cstdio>

namespace esphome {
namespace ez1m {

static const char *const TAG = "ez1m";
static const uint32_t LIFETIME_ENERGY_SAVE_INTERVAL_MS = 3600000;  // 1h, mirrors the original YAML's hourly NVS save

void EZ1MComponent::setup() {
  this->load_lifetime_energy_();
#ifdef USE_NUMBER
  if (this->total_energy_number_ != nullptr)
    this->total_energy_number_->publish_state(this->total_kwh_);
#endif

  this->set_interval("ez1m_save_energy", LIFETIME_ENERGY_SAVE_INTERVAL_MS,
                      [this]() { this->save_lifetime_energy_(); });
}

void EZ1MComponent::update() { this->send_poll_request_(); }

void EZ1MComponent::send_poll_request_() {
  uint8_t frame[] = {0xFB, 0xFB, 0x06, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC1, 0xFE, 0xFE};
  this->write_array(frame, sizeof(frame));
}

void EZ1MComponent::loop() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    this->rx_buffer_.push_back(byte);
    if (this->rx_buffer_.size() > 128)
      this->rx_buffer_.erase(this->rx_buffer_.begin());
  }

  while (this->rx_buffer_.size() >= 3) {
    if (this->rx_buffer_[0] != 0xFB || this->rx_buffer_[1] != 0xFB) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }
    uint8_t len = this->rx_buffer_[2];
    size_t frame_len = (size_t) len + 7;
    if (this->rx_buffer_.size() < frame_len)
      return;  // wait for the rest of the frame
    if (this->rx_buffer_[frame_len - 2] != 0xFE || this->rx_buffer_[frame_len - 1] != 0xFE) {
      ESP_LOGW(TAG, "Bad frame footer, resyncing");
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }
    this->handle_frame_(this->rx_buffer_.data(), frame_len);
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + frame_len);
  }
}

void EZ1MComponent::handle_frame_(const uint8_t *bytes, size_t frame_len) {
  uint8_t len = bytes[2];

  uint16_t chk = 0;
  for (size_t i = 2; i < (size_t) len + 3; i++)
    chk += bytes[i];
  if (bytes[len + 3] != (uint8_t) (chk >> 8) || bytes[len + 4] != (uint8_t) (chk & 0xFF)) {
    ESP_LOGW(TAG, "Checksum mismatch, dropping frame");
    return;
  }

  if (bytes[3] != 0xBB || len < 52)
    return;

  const uint8_t *p = &bytes[4];

#ifdef USE_TEXT_SENSOR
  // DSP firmware version
  char ver[16];
  snprintf(ver, sizeof(ver), "%d.%d", p[2], p[3]);
  this->publish_text_sensor_(EZ1MTextSensorType::DSP_VERSION, ver);
#endif

  // v1, v2 - DC voltage per channel
  uint16_t ch1_v_raw = (p[16] << 8) | p[17];
  uint16_t ch2_v_raw = (p[18] << 8) | p[19];
  float ch1_v = ch1_v_raw / this->dc_voltage_divisor_;
  float ch2_v = ch2_v_raw / this->dc_voltage_divisor_;

  // c1, c2 - DC current per channel
  float ch1_i = ((p[20] << 8) | p[21]) / this->dc_current_divisor_;
  float ch2_i = ((p[22] << 8) | p[23]) / this->dc_current_divisor_;

  // p1, p2 - DC power per channel (V x I)
  float ch1_p = ch1_i * ch1_v;
  float ch2_p = ch2_i * ch2_v;

#ifdef USE_SENSOR
  this->publish_sensor_(EZ1MSensorType::CH1_DC_VOLTAGE, ch1_v);
  this->publish_sensor_(EZ1MSensorType::CH2_DC_VOLTAGE, ch2_v);
  this->publish_sensor_(EZ1MSensorType::CH1_DC_CURRENT, ch1_i);
  this->publish_sensor_(EZ1MSensorType::CH2_DC_CURRENT, ch2_i);
  this->publish_sensor_(EZ1MSensorType::CH1_DC_POWER, ch1_p);
  this->publish_sensor_(EZ1MSensorType::CH2_DC_POWER, ch2_p);
  this->publish_sensor_(EZ1MSensorType::TOTAL_DC_POWER, ch1_p + ch2_p);

  // rtime - inverter uptime (s)
  this->publish_sensor_(EZ1MSensorType::INVERTER_UPTIME, (float) (uint16_t) ((p[28] << 8) | p[29]));

  // AC power output (W)
  this->publish_sensor_(EZ1MSensorType::AC_POWER, (float) (uint16_t) ((p[30] << 8) | p[31]));

  // Temperature (deg C)
  uint16_t temp_raw = (p[32] << 8) | p[33];
  if (temp_raw > 0 && temp_raw < 200)
    this->publish_sensor_(EZ1MSensorType::TEMPERATURE, (float) temp_raw);

  // gf - grid frequency
  uint16_t freq_raw = (p[36] << 8) | p[37];
  if (freq_raw != 0xFFFF) {
    float freq = freq_raw / this->grid_frequency_divisor_;
    if (freq > 45.0f && freq < 55.0f)
      this->publish_sensor_(EZ1MSensorType::GRID_FREQUENCY, freq);
  }
#endif

  // e1, e2 - session energy per channel (resets each morning). Computed
  // unconditionally: lifetime_energy tracking below needs it regardless of
  // whether any sensor entity was declared for it.
  uint32_t e_ch1 = ((uint32_t) p[40] << 24) | ((uint32_t) p[41] << 16) | (p[42] << 8) | p[43];
  uint32_t e_ch2 = ((uint32_t) p[44] << 24) | ((uint32_t) p[45] << 16) | (p[46] << 8) | p[47];
  float ch1_session = e_ch1 / 65536.0f / 1000.0f;
  float ch2_session = e_ch2 / 65536.0f / 1000.0f;
  float daily = ch1_session + ch2_session;

#ifdef USE_SENSOR
  this->publish_sensor_(EZ1MSensorType::CH1_SESSION_ENERGY, ch1_session);
  this->publish_sensor_(EZ1MSensorType::CH2_SESSION_ENERGY, ch2_session);
  this->publish_sensor_(EZ1MSensorType::DAILY_ENERGY, daily);
#endif

  // Lifetime energy: accumulate the delta in RAM, persisted hourly (and on manual edit)
  if (this->prev_daily_ < 0) {
    // first poll after boot - just sync, don't add anything yet
  } else if (daily < this->prev_daily_ - 0.01f) {
    // DSP rebooted (new day) - the counter itself restarted from ~0
    this->total_kwh_ += daily;
  } else {
    this->total_kwh_ += daily - this->prev_daily_;
  }
  this->prev_daily_ = daily;
#ifdef USE_SENSOR
  this->publish_sensor_(EZ1MSensorType::LIFETIME_ENERGY, this->total_kwh_);
#endif
#ifdef USE_NUMBER
  if (this->total_energy_number_ != nullptr)
    this->total_energy_number_->publish_state(this->total_kwh_);
#endif

#if defined(USE_TEXT_SENSOR) || defined(USE_SWITCH)
  // Inverter state byte, needed by the text_sensor and/or the on/off switch
  // (to keep the switch's UI in sync with what the hardware actually reports).
#ifdef USE_TEXT_SENSOR
  std::string state;
  switch (p[48]) {
    case 0x00:
      state = "Producing";
      break;
    case 0x02:
      state = "Standby";
      break;
    case 0x03:
      state = "Ramping up";
      break;
    default: {
      char st[16];
      snprintf(st, sizeof(st), "Unknown 0x%02X", p[48]);
      state = st;
    }
  }
  this->publish_text_sensor_(EZ1MTextSensorType::INVERTER_STATE, state);
#endif
#ifdef USE_SWITCH
  if (this->onoff_switch_ != nullptr)
    this->onoff_switch_->publish_state(p[48] != 0x02);
#endif
#endif  // USE_TEXT_SENSOR || USE_SWITCH

#ifdef USE_NUMBER
  // Power limit readback
  uint16_t mp_raw = (p[50] << 8) | p[51];
  if (mp_raw > 0 && this->power_limit_number_ != nullptr) {
    float disc = 275.6728f + ((float) mp_raw - 300.0f) / 300.0f;
    float watts = roundf((-16.6034f + sqrtf(disc)) * 600.0f);
    watts = fmaxf(30.0f, fminf(800.0f, watts));
    this->power_limit_number_->publish_state(watts);
  }
#endif
}

uint16_t EZ1MComponent::watts_to_raw_(float watts) const {
  float raw_f = (watts * watts) / 1200.0f + 16.6034f * watts + 300.0f;
  return (uint16_t) (raw_f + 0.5f);
}

void EZ1MComponent::set_power_limit(float watts) {
  uint16_t raw = this->watts_to_raw_(watts);
  uint8_t hi = raw >> 8, lo = raw & 0xFF;
  uint16_t chk = 0x06 + 0xAA + 0xE1 + hi + lo;
  uint8_t frame[] = {0xFB, 0xFB, 0x06, 0xAA, 0xE1, 0x00, 0x00, hi, lo,
                      (uint8_t) (chk >> 8), (uint8_t) (chk & 0xFF), 0xFE, 0xFE};
  this->write_array(frame, sizeof(frame));
}

void EZ1MComponent::turn_on(float watts) {
  if (std::isnan(watts) || watts < 30.0f)
    watts = 800.0f;
  this->set_power_limit(watts);
}

void EZ1MComponent::turn_off() {
  uint16_t chk = 0x06 + 0xAA + 0xE1;
  uint8_t frame[] = {0xFB, 0xFB, 0x06, 0xAA, 0xE1, 0x00, 0x00, 0x00, 0x00,
                      (uint8_t) (chk >> 8), (uint8_t) (chk & 0xFF), 0xFE, 0xFE};
  this->write_array(frame, sizeof(frame));
}

float EZ1MComponent::get_power_limit_value() const {
#ifdef USE_NUMBER
  if (this->power_limit_number_ == nullptr)
    return NAN;
  return this->power_limit_number_->state;
#else
  return NAN;
#endif
}

void EZ1MComponent::set_lifetime_energy(float kwh) {
  this->total_kwh_ = kwh;
  this->save_lifetime_energy_();
}

void EZ1MComponent::save_lifetime_energy_() { this->pref_.save(&this->total_kwh_); }

void EZ1MComponent::load_lifetime_energy_() {
  this->pref_ = global_preferences->make_preference<float>(fnv1_hash("ez1m_lifetime_energy"));
  float loaded = 0.0f;
  if (this->pref_.load(&loaded))
    this->total_kwh_ = loaded;
}

#ifdef USE_SENSOR
void EZ1MComponent::register_sensor(EZ1MSensor *sensor, EZ1MSensorType type) {
  this->sensors_.emplace_back(type, sensor);
}

void EZ1MComponent::publish_sensor_(EZ1MSensorType type, float value) {
  for (auto &pr : this->sensors_) {
    if (pr.first == type)
      pr.second->publish_state(value);
  }
}
#endif

#ifdef USE_TEXT_SENSOR
void EZ1MComponent::register_text_sensor(EZ1MTextSensor *sensor, EZ1MTextSensorType type) {
  this->text_sensors_.emplace_back(type, sensor);
}

void EZ1MComponent::publish_text_sensor_(EZ1MTextSensorType type, const std::string &value) {
  for (auto &pr : this->text_sensors_) {
    if (pr.first == type)
      pr.second->publish_state(value);
  }
}
#endif

void EZ1MComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "APsystems EZ1-M:");
  ESP_LOGCONFIG(TAG, "  DC voltage divisor: %.2f", this->dc_voltage_divisor_);
  ESP_LOGCONFIG(TAG, "  DC current divisor: %.2f", this->dc_current_divisor_);
  ESP_LOGCONFIG(TAG, "  Grid frequency divisor: %.2f", this->grid_frequency_divisor_);
  ESP_LOGCONFIG(TAG, "  Lifetime energy: %.3f kWh", this->total_kwh_);
  this->check_uart_settings(57600);
}

}  // namespace ez1m
}  // namespace esphome
