#include "offsr.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

static const char *const TAG = "offsr";
static const float coeffP = 0.001f;
static const float coeffI = 0.0001f;
static const float coeffD = 0.001f;
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
      // this->pid_update();
    });
    this->current_battery_voltage_ = this->battery_voltage_sensor_->state;
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->current_power_ = state;
      // this->pid_update();
    });
    this->current_power_ = this->power_sensor_->state;
  }
  
  this->pid_computed_callback_.call();
  // this->pid_update();
  
  ESP_LOGVV(TAG, "setup: battery_current=%3.2f, battery_voltage=%3.2f, power_sensor=%3.2f, pid_mode = %d", this->current_battery_current_ , this->current_battery_voltage_ , this->current_power_ , this->current_pid_mode_);  
  
}

void OFFSRComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  ESP_LOGVV(TAG, "setup import part: battery_current=%3.2f, battery_voltage=%3.2f, power_sensor=%3.2f", this->current_battery_current_ , this->current_battery_voltage_ , this->current_power_);
    
  this->pid_computed_callback_.call(); 
}


void OFFSRComponent::pid_update() {
  uint32_t now = millis();
  float tmp;
  float alphaP, alphaI, alphaD, alpha;
  
  ESP_LOGVV(TAG, "Entered in pid_update()");
  ESP_LOGVV(TAG, "Current pid mode %d" , this->current_pid_mode_);
    
  if(this->current_battery_voltage_ < this->current_discharged_battery_voltage_){
	  this->current_target_ = this->current_charging_setpoint_;
  }
  else if((this->current_battery_voltage_ >= this->current_discharged_battery_voltage_) && (this->current_battery_voltage_ < this->current_charged_battery_voltage_)){
	  this->current_target_ = this->current_absorbing_setpoint_;
  }
  else{
      this->current_target_ = this->current_floating_setpoint_;
  }
#ifdef USE_SWITCH  
  if (!this->current_manual_override_){
#endif
    dt_ = float(now - this->last_time_)/1000.0f;
    this->error_ = -(this->current_target_ - this->current_battery_current_);
#ifdef USE_SWITCH	  
	if (this->current_reverse_){
		error_ = -error_;
	}
#endif	  
	this->current_error_ = this->error_;
	
    tmp = (this->error_ * dt_);
    if (!std::isnan(tmp)){
      integral_ += tmp;
    }
    this->derivative_ = (this->error_ - this->previous_error_) / dt_;

    tmp = 0.0f;
    if( !std::isnan(previous_output_) && !this->current_pid_mode_){
        tmp = previous_output_;
    }
	
	ESP_LOGVV(TAG, "previous output = %2.8f" , tmp );
	ESP_LOGVV(TAG, "E = %3.2f, I = %3.2f, D = %3.2f, previous = %3.2f" , error_ , integral_ , derivative_ , tmp);
	
	alphaP = coeffP*this->current_kp_ * this->error_;
	alphaI = coeffI*this->current_ki_ * this->integral_;
	alphaD = coeffD*this->current_kd_ * this->derivative_;
	alpha  = alphaP + alphaI + alphaD;
	
    output_ = std::min(std::max( tmp + alpha, this->current_output_min_  ) , this->current_output_max_);
	
    ESP_LOGVV(TAG, "Pcoeff = %3.8f" , alphaP );
	ESP_LOGVV(TAG, "Icoeff = %3.8f" , alphaI );
	ESP_LOGVV(TAG, "Dcoeff = %3.8f" , alphaD );
	
	ESP_LOGVV(TAG, "output_min = %1.2f" , this->current_output_min_  );
	ESP_LOGVV(TAG, "output_max = %1.2f" , this->current_output_max_  );
	
	ESP_LOGVV(TAG, "PIDcoeff = %3.8f" , alpha );
	
	ESP_LOGVV(TAG, "Intermediate computed output=%1.6f" , this->output_);
  
    // if ( (!std::isnan(this->current_power_)) && (this->current_power_ < power_mini) &&  (this->previous_output_ >= this->current_output_restart_) ) {
    if ( (this->power_sensor_ != nullptr) && (this->current_power_ < power_mini) &&  (this->previous_output_ >= this->current_output_restart_) ) {  		
		output_ = this->current_output_restart_;
#ifdef USE_BINARY_SENSOR 	  
      this->current_thermostat_cut_= true;
#endif
      ESP_LOGVV(TAG, "restart  output");
    }
    else{
#ifdef USE_BINARY_SENSOR 	  
      this->current_thermostat_cut_ = false;
#endif	
      ESP_LOGVV(TAG, "full pid update: setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_output_ = %3.2f , output_ = %3.2f , error_ = %3.2f, integral = %3.2f , derivative = %3.2f, current_power = %3.2f", this->current_target_ , coeff*this->current_kp_ , coeff*this->current_ki_ , coeff*this->current_kd_ , this->current_output_min_ , this->current_output_max_ , previous_output_ , output_ , error_ , integral_ , derivative_ , this->current_power_);  
    }
  
    last_time_ = now;
    this->previous_error_ = this->error_;
    this->previous_output_ = this->output_;
    
	ESP_LOGVV(TAG, "activation %d", current_activation_);
	
#ifdef USE_SWITCH  
    if (!this->current_activation_ ){
      output_ = 0.0f;
    }
#endif  

    if (!std::isnan(this->current_battery_voltage_)){
	  ESP_LOGVV(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f" , this->current_battery_voltage_, this->current_starting_battery_voltage_);	
      if (this->current_battery_voltage_ < this->current_starting_battery_voltage_){
        output_ = 0.0f;
      }
    }
    ESP_LOGVV(TAG, "Final computed output=%1.6f" , this->output_);
	
    this->device_output_->set_level(this->output_);
	this->current_output_ = this->output_;
    this->pid_computed_callback_.call();
#ifdef USE_SWITCH	
  } 
#endif  
  
 }

 }  // namespace offsr
}  // namespace esphome





