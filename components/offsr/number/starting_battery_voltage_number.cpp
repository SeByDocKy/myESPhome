#include "starting_battery_voltage_number.h"

namespace esphome {
namespace offsr {

void StartingBatteryVoltageNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_starting_battery_voltage(value);
}

}  // namespace offsr
}  // namespace esphome
