#include "offsr.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

static const char *const TAG = "offsr";

// /*
void OFFSRComponent::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up OFFSRComponent...");
  
  if (this->battery_current_sensor_ != nullptr) {
    this->battery_current_sensor_->add_on_state_callback([this](float state) {
      this->current_battery_current_ = state;
    });
    this->current_battery_current_ = this->battery_current_sensor_->state;
  }
  if (this->battery_voltage_sensor_ != nullptr) {
    this->battery_voltage_sensor_->add_on_state_callback([this](float state) {
      this->current_battery_voltage_ = state;
    });
    this->current_battery_voltage_ = this->battery_voltage_sensor_->state;
  }
  
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->current_power_ = state;
    });
    this->current_power_ = this->power_sensor_->state;
  }
}

void OFFSRComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  // LOG_SENSOR("", "device output", this->device_output_);
}


void OFFSRComponent::pid_update() {
	uint32_t now = millis();	
 }

#ifdef USE_SWITCH
void OFFSRComponent::set_activation(bool enable) {
	this->current_activation_ = enable; 
}
void OFFSRComponent::set_manual_override(bool enable) {
	this->current_manual_override_ = enable;
}
#endif


#ifdef USE_NUMBER
void OFFSRComponent::set_charging_setpoint(float value) {
	this->current_charging_setpoint_ = value; 
}
void OFFSRComponent::set_absorbing_setpoint(float value) {
	this->current_absorbing_setpoint_ = value;
}
void OFFSRComponent::set_floating_setpoint(float value) {
	this->current_floating_setpoint_ = value;
}

void OFFSRComponent::set_starting_battery_voltage(float value) {
	this->current_starting_battery_voltage_ = value;
}
void OFFSRComponent::set_charged_battery_voltage(float value) {
	this->current_charged_battery_voltage_ = value;
}
void OFFSRComponent::set_discharged_battery_voltage(float value) {
	this->current_discharged_battery_voltage_ = value;
}

void OFFSRComponent::set_kp(float value) {
	this->current_kp_ = value;
}
void OFFSRComponent::set_ki(float value) {
	this->current_ki_ = value;
}
void OFFSRComponent::set_kd(float value) {
	this->current_kd_ = value;
}

void OFFSRComponent::set_output_min(float value) {
	this->current_output_min_ = value;
}
void OFFSRComponent::set_output_max(float value) {
	this->current_output_max_ = value;
}
void OFFSRComponent::set_output_restart(float value) {
	this->current_output_restart_ = value;
}

#endif



/*
#ifdef USE_BINARY_SENSOR
void OFFSRComponent::get_thermostat_cut(bool state) {
// void OFFSRComponent::set_thermostat_cut(bool state) {
// void OFFSRComponent::set_thermostat_cut(binary_sensor::BinarySensor *bs) {
	// this->current_thermostat_cut_ = bs; 
	this->current_thermostat_cut_ = state; 
	// this->thermostat_cut_binary_sensor_ = bs;
}
#endif
*/

/*
#ifdef USE_SENSOR	

void OFFSRComponent::get_error(float error) {
	this->current_error_ = error;
}
void OFFSRComponent::get_output(float output) {
	this->current_output_ = output;
}
*/
/*
void OFFSRComponent::set_error(float error) {
	this->current_error_ = error;
}
void OFFSRComponent::set_output(float output) {
	this->current_output_ = output;
}
*/
/*
void OFFSRComponent::set_error(sensor::Sensor *error_sensor) {
	this->current_error_ = error_sensor;
}
void OFFSRComponent::set_output(sensor::Sensor *output_sensor) {
	this->current_output_ = output_sensor;
}

#endif
*/
 }  // namespace offsr
}  // namespace esphome