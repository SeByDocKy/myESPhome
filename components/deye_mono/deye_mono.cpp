#include "deye_mono.h"

namespace esphome::deye_mono {

static const char *const TAG = "deye_mono";

void DeyeMonoHub::setup() {
  if (this->controller_ == nullptr) {
    ESP_LOGE(TAG, "No modbus_controller: nothing to talk to the inverter with");
    this->mark_failed();
    return;
  }
  this->writer_.init(this->controller_);
  // Nothing to do in loop(): the entities are driven by the controller
  this->disable_loop();
}

void DeyeMonoHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Deye inverter:\n"
                "  Inverter factor: %g\n"
                "  Write function: %s\n"
                "  Write hold: %u ms\n"
                "  Tracked registers: %u",
                static_cast<double>(this->inverter_factor_),
                this->use_write_multiple_ ? "0x10 (write multiple registers)" : "0x06 (write single register)",
                static_cast<unsigned>(this->write_hold_ms_), static_cast<unsigned>(this->registers_.size()));
}

void DeyeMonoHub::add_item(SensorItem *item) {
  if (this->controller_ != nullptr)
    this->controller_->add_sensor_item(item);
}

DeyeMonoHub::RegisterState *DeyeMonoHub::find_(uint16_t reg) {
  for (auto &state : this->registers_) {
    if (state.reg == reg)
      return &state;
  }
  return nullptr;
}

void DeyeMonoHub::track_register(uint16_t reg) {
  if (this->find_(reg) == nullptr)
    this->registers_.push_back(RegisterState{reg, 0, 0, false, false});
}

bool DeyeMonoHub::accept_read(uint16_t reg, uint16_t value) {
  RegisterState *state = this->find_(reg);
  if (state == nullptr)
    return true;
  if (state->hold) {
    if (static_cast<int32_t>(millis() - state->hold_until) < 0)
      return false;
    state->hold = false;
  }
  state->value = value;
  state->valid = true;
  return true;
}

bool DeyeMonoHub::write_register(uint16_t reg, uint16_t value) {
  RegisterState *state = this->find_(reg);
  // Already written with this very value and not read back yet: nothing to send (and the bus would refuse a frame
  // identical to the one it still has)
  if (state != nullptr && state->hold && state->value == value &&
      static_cast<int32_t>(millis() - state->hold_until) < 0) {
    ESP_LOGD(TAG, "Register %u already written with %u", reg, value);
    return true;
  }
  ESP_LOGD(TAG, "Write register %u = %u (0x%04X)", reg, value, value);
  const bool queued = this->use_write_multiple_
                          ? this->writer_.write_multiple_registers(reg, std::span<const uint16_t>(&value, 1))
                          : this->writer_.write_single_register(reg, value);
  if (!queued) {
    ESP_LOGW(TAG, "Write of register %u refused by the Modbus bus (queue full?)", reg);
    return false;
  }
  if (state != nullptr) {
    state->value = value;
    state->valid = true;
    state->hold = this->write_hold_ms_ != 0;
    state->hold_until = millis() + this->write_hold_ms_;
  }
  return true;
}

bool DeyeMonoHub::write_masked(uint16_t reg, uint16_t mask, uint16_t bits) {
  RegisterState *state = this->find_(reg);
  if (state == nullptr || !state->valid) {
    ESP_LOGW(TAG, "Register %u not read yet: not writing, the other bits of it are not known", reg);
    return false;
  }
  return this->write_register(reg, static_cast<uint16_t>((state->value & ~mask) | (bits & mask)));
}

}  // namespace esphome::deye_mono
