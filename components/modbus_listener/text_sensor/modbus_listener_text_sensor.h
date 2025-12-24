#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../modbus_listener.h"

namespace esphome {
namespace modbus_listener {

class ModbusListenerTextSensor : public Component {
 public:
  // Configuration
  void set_parent(ModbusListenerHub *parent) { parent_ = parent; }
  void set_tx_sensor(text_sensor::TextSensor *sensor) { tx_sensor_ = sensor; }
  void set_rx_sensor(text_sensor::TextSensor *sensor) { rx_sensor_ = sensor; }
  
  // Appelé par le hub quand une trame est reçue
  void update_frame(const std::string &hex_data, FrameType type);
  
  // Component overrides
  void setup() override;
  void dump_config() override;

 protected:
  ModbusListenerHub *parent_{nullptr};
  text_sensor::TextSensor *tx_sensor_{nullptr};
  text_sensor::TextSensor *rx_sensor_{nullptr};
};

}  // namespace modbus_listener
}  // namespace esphome