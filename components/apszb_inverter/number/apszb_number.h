#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/number/number.h"
#include "esphome/components/apszb/apszb.h"

namespace esphome {
namespace apszb {

// Power limit (throttling) of one inverter, in watts per inverter output as in the original ESP32-ECU firmware.
class APSMaxPowerNumber : public number::Number, public Parented<APSInverter> {
 protected:
  void control(float value) override {
    this->publish_state(value);  // optimistic, replaced by the value read back from the inverter
    this->parent_->request_max_power(value);
  }
};

}  // namespace apszb
}  // namespace esphome
