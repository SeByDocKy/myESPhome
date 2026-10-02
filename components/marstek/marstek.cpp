#include "marstek.h"

namespace esphome::marstek {

static const char *const TAG = "marstek";

void MarstekDepReader::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data))
    return;
  this->value_ = static_cast<float>(static_cast<double>(this->raw_(data)) * static_cast<double>(this->scale_));
  this->valid_ = true;
  if (this->callback_)
    this->callback_();
}

void MarstekHub::setup() {
  // Builds the read ranges from the registered entities. Everything registers at code generation time, before
  // any setup() runs, so the list is complete here.
  modbustcp_controller::ModbusTCPController::setup();

  this->add_on_online_callback([this](int, int) { this->set_connected_(true); });
  this->add_on_offline_callback([this](int, int) { this->set_connected_(false); });
  this->set_connected_(false);
}

void MarstekHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Marstek:\n"
                "  Model: %s\n"
                "  Auto RS485 control mode: %s",
                this->model_, YESNO(this->auto_rs485_control_));
  modbustcp_controller::ModbusTCPController::dump_config();
}

void MarstekHub::set_connected_(bool connected) {
  if (this->connected_known_ && this->connected_ == connected)
    return;
  this->connected_known_ = true;
  this->connected_ = connected;
  if (connected) {
    ESP_LOGI(TAG, "Battery answers (%s)", this->model_);
  }
#ifdef USE_BINARY_SENSOR
  if (this->connection_sensor_ != nullptr)
    this->connection_sensor_->publish_state(connected);
#endif
}

// The controller does not mark itself online on the very first reply (it only reports offline -> online
// transitions), so the hub notes every valid reply, and every Modbus exception (the battery is there and talking).
void MarstekHub::on_modbus_data(const std::vector<uint8_t> &data) {
  modbustcp_controller::ModbusTCPController::on_modbus_data(data);
  this->set_connected_(true);
}

void MarstekHub::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
  modbustcp_controller::ModbusTCPController::on_modbus_error(function_code, exception_code);
  this->set_connected_(true);
}

void MarstekHub::write_register(uint16_t reg, uint16_t value) {
  if (this->auto_rs485_control_ && reg > REG_RS485_CONTROL_MODE && reg <= REG_CONTROLLED_LAST &&
      !this->rs485_enabled_) {
    ESP_LOGD(TAG, "Enabling RS485 control mode before writing register %u", reg);
    this->queue_command(modbustcp_controller::ModbusCommandItem::create_write_single_command(
        this, REG_RS485_CONTROL_MODE, RS485_CONTROL_ENABLE));
    this->rs485_enabled_ = true;
  }
  if (reg == REG_RS485_CONTROL_MODE)
    this->rs485_enabled_ = (value == RS485_CONTROL_ENABLE);

  ESP_LOGD(TAG, "Write register %u = %u (0x%04X)", reg, value, value);
  this->queue_command(modbustcp_controller::ModbusCommandItem::create_write_single_command(this, reg, value));
}

}  // namespace esphome::marstek
