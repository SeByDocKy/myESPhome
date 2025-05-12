#include "offsr.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

static const char *const TAG = "offsr";
static const float coeff = 0.001f;
static const float power_mini = 2.0f;

void OFFSRComponent::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up OFFSRComponent...");
  
  last_time_ =  millis();
  integral_  = 0.0f;
  previous_output_ = 0.0f;
  previous_error_ = 0.0f;
  
  if (this->battery_current_sensor_ != nullptr) {
    this->battery_current_sensor_->add_on_state_callback([this](float state) {
      this->current_battery_current_ = state;
      this->pid_update();
    });
    this->current_battery_current_ = this->battery_current_sensor_->state;
  }
  if (this->battery_voltage_sensor_ != nullptr) {
    this->battery_voltage_sensor_->add_on_state_callback([this](float state) {
      this->current_battery_voltage_ = state;
      this->pid_update();
    });
    this->current_battery_voltage_ = this->battery_voltage_sensor_->state;
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->current_power_ = state;
      this->pid_update();
    });
    this->current_power_ = this->power_sensor_->state;
  }
  
  this->pid_computed_callback_.call();
  this->pid_update();
  
  ESP_LOGV(TAG, "setup: battery_current=%3.2f, battery_voltage=%3.2f, power_sensor=%3.2f, pid_mode = %d", this->current_battery_current_ , this->current_battery_voltage_ , this->current_power_ , this->current_pid_mode_);  
  
}

void OFFSRComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  ESP_LOGV(TAG, "setup import part: battery_current=%3.2f, battery_voltage=%3.2f, power_sensor=%3.2f, pid_mode = %d", this->current_battery_current_ , this->current_battery_voltage_ , this->current_power_ , this->current_pid_mode_);
  
  ESP_LOGV(TAG, "setup numbers: manual_level=%3.2f, charging_setpoint=%3.2f, absorbing_setpoint=%3.2f, floating_setpoint = %3.2f", this->current_manual_level_ , this->current_charging_setpoint_ , this->current_absorbing_setpoint_ , this->current_floating_setpoint_);
  
  ESP_LOGV(TAG, "setup switches: activation=%d, overide=%d", this->current_activation_ , this->current_manual_override_);  
  
  ESP_LOGV(TAG, "setup sensors part: error=%3.2f, output=%3.2f, target=%3.2f", this->current_error_ , this->current_output_ , this->current_target_);
  
  this->pid_computed_callback_.call();
  
  
}


void OFFSRComponent::pid_update() {
  uint32_t now = millis();
  float tmp;
  
  ESP_LOGV(TAG, "Entered in pid_update()");
    
  if(this->current_battery_voltage_ <= this->current_discharged_battery_voltage_){
	  this->current_target_ = this->current_charging_setpoint_;
  }
  else if((this->current_battery_voltage_ > this->current_discharged_battery_voltage_) && (this->current_battery_voltage_ <= this->current_charged_battery_voltage_)){
	  this->current_target_ = this->current_absorbing_setpoint_;
  }
  else{
      this->current_target_ = this->current_floating_setpoint_;
  }
#ifdef USE_SWITCH  
  if (!this->current_manual_override_){
#endif
    dt_ = float(now - this->last_time_)/1000.0f;
    error_ = -(this->current_target_ - this->current_battery_current_);
	this->current_error_ = error_;
	
	// ESP_LOGV(TAG, "Initial error=%3.2f" , error_);
	
    tmp = (error_ * dt_);
    if (!std::isnan(tmp)){
      integral_ += tmp;
    }
    derivative_ = (error_ - previous_error_) / dt_;
    tmp = 0.0f;
    if( !std::isnan(previous_output_) && this->current_pid_mode_){
        tmp = previous_output_;
    }
	
	ESP_LOGV(TAG, "E = %3.2f, I = %3.2f, D = %3.2f, previous = %3.2f" , error_ , integral_ , derivative_ , tmp);
	
    output_ = std::min(std::max( tmp + (coeff*this->current_kp_ * error_) + (coeff*this->current_ki_ * integral_) + (coeff*this->current_kd_ * derivative_) , this->current_output_min_  ) , this->current_output_max_);
	
    ESP_LOGV(TAG, "Pcoeff = %3.8f" , coeff*this->current_kp_ * error_ );
	ESP_LOGV(TAG, "Icoeff = %3.8f" , (coeff*this->current_ki_ * integral_) );
	ESP_LOGV(TAG, "Dcoeff = %3.8f" , (coeff*this->current_kd_ * derivative_) );
	
	ESP_LOGV(TAG, "previous output = %2.8f" , tmp );
	
	ESP_LOGV(TAG, "output_min = %1.2f" , this->current_output_min_  );
	ESP_LOGV(TAG, "output_max = %1.2f" , this->current_output_max_  );
	
	ESP_LOGV(TAG, "PIDcoeff = %3.8f" , tmp + (coeff*this->current_kp_ * error_) + (coeff*this->current_ki_ * integral_) + (coeff*this->current_kd_ * derivative_) );
	
	ESP_LOGV(TAG, "Intermediate computed output=%1.6f" , output_);
  
    if ( (!std::isnan(this->current_power_)) && (this->current_power_ < power_mini) &&  (this->previous_output_ >= this->current_output_restart_) ) {
      output_ = this->current_output_restart_;
// #ifdef USE_BINARY_SENSOR 	  
      this->current_thermostat_cut_= true;
// #endif
      ESP_LOGVV(TAG, "restart  output");
    }
    else{
// #ifdef USE_BINARY_SENSOR 	  
      this->current_thermostat_cut_ = false;
// #endif	
      ESP_LOGVV(TAG, "full pid update: setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_output_ = %3.2f , output_ = %3.2f , error_ = %3.2f, integral = %3.2f , derivative = %3.2f, current_power = %3.2f", this->current_target_ , coeff*this->current_kp_ , coeff*this->current_ki_ , coeff*this->current_kd_ , this->current_output_min_ , this->current_output_max_ , previous_output_ , output_ , error_ , integral_ , derivative_ , this->current_power_);  
    }
  
    last_time_ = now;
    previous_error_ = error_;
    previous_output_ = output_;
  
#ifdef USE_SWITCH  
    if (!this->current_activation_ ){
      output_ = 0.0f;
    }
#endif  

    if (!std::isnan(this->current_battery_voltage_)){
	  ESP_LOGV(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f" , this->current_battery_voltage_, this->current_starting_battery_voltage_);	
      if (this->current_battery_voltage_ < this->current_starting_battery_voltage_){
        output_ = 0.0f;
      }
    }
    ESP_LOGV(TAG, "Final computed output=%1.6f" , output_);
    this->device_output_->set_level(output_);
    current_output_ = output_;
    this->pid_computed_callback_.call();	
#ifdef USE_SWITCH	
  }
#else 
  // output_ = this->current_manual_level_;
  // this->device_output_->set_level(get_manual_level());	
#endif  
  //ESP_LOGV(TAG, "Thermostat_cut=%d" , this->current_thermostat_cut_);
  
  
  
 }


 }  // namespace offsr
}  // namespace esphome
