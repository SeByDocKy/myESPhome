#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/modbustcp_controller/modbustcp_controller.h"

namespace esphome {
namespace modbustcp_controller {

class ModbusCommandSentTrigger : public Trigger<int, int> {
 public:
  ModbusCommandSentTrigger(ModbusTCPController *a_modbuscontroller) {
    a_modbuscontroller->add_on_command_sent_callback(
        [this](int function_code, int address) { this->trigger(function_code, address); });
  }
};

class ModbusOnlineTrigger : public Trigger<int, int> {
 public:
  ModbusOnlineTrigger(ModbusTCPController *a_modbuscontroller) {
    a_modbuscontroller->add_on_online_callback(
        [this](int function_code, int address) { this->trigger(function_code, address); });
  }
};

class ModbusOfflineTrigger : public Trigger<int, int> {
 public:
  ModbusOfflineTrigger(ModbusTCPController *a_modbuscontroller) {
    a_modbuscontroller->add_on_offline_callback(
        [this](int function_code, int address) { this->trigger(function_code, address); });
  }
};

}  // namespace modbustcp_controller
}  // namespace esphome
