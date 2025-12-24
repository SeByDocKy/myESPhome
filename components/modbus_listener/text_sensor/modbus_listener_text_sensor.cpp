#include "modbus_listener_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_listener {

static const char *const TAG = "modbus_listener.text_sensor";

void ModbusListenerTextSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Modbus Listener Text Sensor...");
}

void ModbusListenerTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Listener Text Sensor:");
  if (tx_sensor_ != nullptr) {
    LOG_TEXT_SENSOR("  ", "TX Sensor", tx_sensor_);
  }
  if (rx_sensor_ != nullptr) {
    LOG_TEXT_SENSOR("  ", "RX Sensor", rx_sensor_);
  }
}

void ModbusListenerTextSensor::update_frame(const std::string &hex_data, FrameType type) {
  if (type == FRAME_REQUEST && tx_sensor_ != nullptr) {
    ESP_LOGI(TAG, "TX (Request): %s", hex_data.c_str());
    tx_sensor_->publish_state(hex_data);
  } else if (type == FRAME_RESPONSE && rx_sensor_ != nullptr) {
    ESP_LOGI(TAG, "RX (Response): %s", hex_data.c_str());
    rx_sensor_->publish_state(hex_data);
  }
}

}  // namespace modbus_listener
}  // namespace esphome