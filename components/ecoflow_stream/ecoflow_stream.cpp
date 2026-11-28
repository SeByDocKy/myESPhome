#include "ecoflow_stream.h"

#include <algorithm>

namespace esphome::ecoflow_stream {

static const char *const TAG = "ecoflow_stream";

void EcoflowStreamHub::setup() {
  if (this->controller_ == nullptr) {
    ESP_LOGE(TAG, "No modbus_controller: nothing to talk to the device with");
    this->mark_failed();
    return;
  }
  this->heartbeat_writer_.init(this->controller_);
  this->control_point_writer_.init(this->controller_);
  this->setpoint_writer_.init(this->controller_);

  // The hub's own readers are added here, before the controllers build their read ranges (this hub's setup priority
  // is higher than theirs)
  this->master_reader_.configure(REG_MASTER_FLAG, 1, SensorValueType::U_WORD, EntityType::HOLDING);
  this->master_reader_.set_callback([this](int64_t value) {
    this->set_connected_(true);
    const int32_t master = value == 1 ? 1 : 0;
    if (master != this->master_) {
      ESP_LOGI(TAG, "This unit is %s", master ? "the system master" : "NOT the system master");
      if (!master && !this->ignore_master_flag_)
        ESP_LOGW(TAG, "System values are neither published nor written: point the component to the master");
      this->master_ = master;
    }
  });
  this->add_item(&this->master_reader_, false);

  if (this->outputs_enabled_) {
    this->control_point_reader_.configure(REG_CONTROL_POINT, 1, SensorValueType::U_WORD, EntityType::HOLDING);
    this->control_point_reader_.set_system_value(this);
    this->control_point_reader_.set_callback(
        [this](int64_t value) { this->device_control_point_ = static_cast<int32_t>(value); });
    this->add_item(&this->control_point_reader_, false);

    this->max_output_reader_.configure(REG_MAX_OUTPUT_POWER, 2, SensorValueType::U_DWORD, EntityType::HOLDING);
    this->max_output_reader_.set_system_value(this);
    this->max_output_reader_.set_callback(
        [this](int64_t value) { this->max_output_w_ = static_cast<int32_t>(value); });
    this->add_item(&this->max_output_reader_, false);

    this->max_input_reader_.configure(REG_MAX_INPUT_POWER, 2, SensorValueType::U_DWORD, EntityType::HOLDING);
    this->max_input_reader_.set_system_value(this);
    this->max_input_reader_.set_callback(
        [this](int64_t value) { this->max_input_w_ = static_cast<int32_t>(value); });
    this->add_item(&this->max_input_reader_, false);
  }

  // The items declared by the platforms are handed over here: the controllers are all known now
  ModbusController *slow = this->slow_controller_ != nullptr ? this->slow_controller_ : this->controller_;
  for (SensorItem *item : this->fast_items_)
    this->controller_->add_sensor_item(item);
  for (SensorItem *item : this->slow_items_)
    slow->add_sensor_item(item);

  this->controller_->add_on_online_callback([this](int, int) { this->set_connected_(true); });
  this->controller_->add_on_offline_callback([this](int, int) { this->set_connected_(false); });
  this->set_connected_(false);
}

void EcoflowStreamHub::add_item(SensorItem *item, bool slow) { (slow ? this->slow_items_ : this->fast_items_).push_back(item); }

void EcoflowStreamHub::loop() {
  if (!this->connected_)
    return;
  const uint32_t now = millis();
  if (this->control_enabled_ && this->heartbeat_enabled_ && this->heartbeat_interval_ms_ != 0 &&
      (!this->heartbeat_sent_ || now - this->last_heartbeat_ms_ >= this->heartbeat_interval_ms_)) {
    this->send_heartbeat_();
  }
  if (this->setpoint_refresh_ms_ != 0 && this->setpoint_valid_ && this->heartbeat_enabled_ &&
      now - this->last_setpoint_ms_ >= this->setpoint_refresh_ms_) {
    this->send_setpoint_();
  }
}

void EcoflowStreamHub::send_heartbeat_() {
  if (!this->heartbeat_writer_.write_single_register(REG_HEARTBEAT, 1))
    ESP_LOGW(TAG, "Heartbeat refused by the Modbus bus (queue full?)");
  this->heartbeat_sent_ = true;
  this->last_heartbeat_ms_ = millis();
}

void EcoflowStreamHub::set_heartbeat_enabled(bool enabled) {
  this->heartbeat_enabled_ = enabled;
  this->heartbeat_sent_ = false;  // send one at once when enabled again
  if (!enabled) {
    this->setpoint_valid_ = false;
    ESP_LOGI(TAG, "Heartbeat stopped: the battery goes back to its own energy management within 60 s");
  }
}

int32_t EcoflowStreamHub::effective_max_(int32_t device, uint32_t override_w) const {
  int32_t limit = device > 0 ? device : 0;
  if (override_w > 0 && (limit == 0 || static_cast<int32_t>(override_w) < limit))
    limit = static_cast<int32_t>(override_w);
  return std::min<int32_t>(limit, 32767);  // 0 = unknown
}

void EcoflowStreamHub::set_power_fraction(bool charge, float fraction) {
  if (std::isnan(fraction))
    fraction = 0.0f;
  fraction = std::min(std::max(fraction, 0.0f), 1.0f);
  (charge ? this->charge_fraction_ : this->discharge_fraction_) = fraction;

  const int32_t max_charge = this->effective_max_(this->max_input_w_, this->max_charge_override_w_);
  const int32_t max_discharge = this->effective_max_(this->max_output_w_, this->max_discharge_override_w_);
  if ((this->charge_fraction_ > 0.0f && max_charge == 0) ||
      (this->discharge_fraction_ > 0.0f && max_discharge == 0)) {
    ESP_LOGW(TAG, "Maximum power not known yet (and not set in the YAML): setpoint not sent");
    return;
  }
  this->setpoint_w_ = static_cast<int32_t>(std::lround(this->discharge_fraction_ * max_discharge -
                                                       this->charge_fraction_ * max_charge));
  this->setpoint_valid_ = true;
  this->send_setpoint_();
}

void EcoflowStreamHub::send_setpoint_() {
  if (!this->system_valid()) {
    ESP_LOGW(TAG, "Setpoint not sent: this unit is not (yet) known as the system master");
    return;
  }
  if (this->charge_sign_ == 0)
    return;  // rejected at validation time

  const int32_t wanted = static_cast<int32_t>(this->control_point_);
  if (this->device_control_point_ != wanted) {
    if (this->auto_control_point_) {
      this->control_point_writer_.write_single_register(REG_CONTROL_POINT, static_cast<uint16_t>(wanted));
      this->control_point_reader_.hold_for(WRITE_HOLD_MS);
      this->device_control_point_ = wanted;
    } else {
      ESP_LOGW(TAG, "Control point of the device is %d, not %d: select it, or set auto_control_point",
               static_cast<int>(this->device_control_point_), static_cast<int>(wanted));
    }
  }

  // setpoint_w_ is positive when discharging; the device charges with the sign `charge_sign_`
  const int32_t value = this->setpoint_w_ * -this->charge_sign_;
  const uint16_t reg = static_cast<uint16_t>(REG_TARGET_BASE + wanted);
  ESP_LOGD(TAG, "Write setpoint: register %u = %d W", reg, static_cast<int>(value));
  if (!this->setpoint_writer_.write_single_register(reg, static_cast<uint16_t>(static_cast<int16_t>(value))))
    ESP_LOGW(TAG, "Write of the setpoint refused by the Modbus bus (queue full?)");
  this->last_setpoint_ms_ = millis();
}

void EcoflowStreamHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "EcoFlow STREAM:\n"
                "  Model: %s\n"
                "  Slow controller: %s\n"
                "  Heartbeat: %s, every %u ms\n"
                "  Ignore master flag: %s",
                this->model_,
                YESNO(this->slow_controller_ != nullptr && this->slow_controller_ != this->controller_),
                YESNO(this->control_enabled_ && this->heartbeat_interval_ms_ != 0),
                static_cast<unsigned>(this->heartbeat_interval_ms_), YESNO(this->ignore_master_flag_));
  if (this->outputs_enabled_) {
    ESP_LOGCONFIG(TAG,
                  "  Outputs: control point %d, charge sign %d, auto control point %s\n"
                  "  Max charge / discharge override: %u / %u W, setpoint refresh: %u ms",
                  static_cast<int>(this->control_point_), static_cast<int>(this->charge_sign_),
                  YESNO(this->auto_control_point_), static_cast<unsigned>(this->max_charge_override_w_),
                  static_cast<unsigned>(this->max_discharge_override_w_),
                  static_cast<unsigned>(this->setpoint_refresh_ms_));
  }
}

void EcoflowStreamHub::set_connected_(bool connected) {
  if (this->connected_known_ && this->connected_ == connected)
    return;
  this->connected_known_ = true;
  this->connected_ = connected;
  if (connected) {
    ESP_LOGI(TAG, "Device answers (%s)", this->model_);
  } else {
    this->master_ = -1;
    this->heartbeat_sent_ = false;
  }
#ifdef USE_BINARY_SENSOR
  if (this->connection_sensor_ != nullptr)
    this->connection_sensor_->publish_state(connected);
#endif
}

}  // namespace esphome::ecoflow_stream
