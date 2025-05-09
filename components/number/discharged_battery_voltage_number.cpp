#include "discharged_battery_voltage_number.h"

namespace esphome {
namespace offsr {

void DischargedBatteryVoltageNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_discharged_battery_voltage(value);
}

}  // namespace offsr
}  // namespace esphome
