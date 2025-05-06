#include "charged_battery_voltage_number"

namespace esphome {
namespace offsr {

void ChargedBatteryVoltageNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charged_battery_voltage(value);
}

}  // namespace offsr
}  // namespace esphome
