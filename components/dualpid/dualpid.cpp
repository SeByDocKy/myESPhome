#include "dualpid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpid {

static const char *const TAG = "dualpid";
static const float coeffP = 0.001f;
static const float coeffI = 0.0001f;
static const float coeffD = 0.001f;

void DUALPIDComponent::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up DUALPIDComponent...");
  
  this->last_time_ =  millis();
  this->integral_  = 0.0f;
  this->previous_output_ = 0.0f;
  this->previous_error_ = 0.0f;
  
  if (this->input_sensor_ != nullptr) {
    this->input_sensor_->add_on_state_callback([this](float state) {
      this->current_input_ = state;
      this->pid_update();
    });
    this->current_input_ = this->input_sensor_->state;
  }
  
  if (this->battery_voltage_sensor_ != nullptr) {
    this->battery_voltage_sensor_->add_on_state_callback([this](float state) {
      this->current_battery_voltage_ = state;
      // this->pid_update();
    });
    this->current_battery_voltage_ = this->battery_voltage_sensor_->state;
  }
  
  this->pid_computed_callback_.call();
  // this->pid_update();
  
  ESP_LOGI(TAG, "setup: battery_voltage=%3.2f, pid_mode = %d", this->current_battery_voltage_ , this->current_pid_mode_);  
  
}

void DUALPIDComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  ESP_LOGVV(TAG, "setup import part: battery_voltage=%3.2f", this->current_battery_voltage_ );
  this->pid_computed_callback_.call(); 
}

void DUALPIDComponent::pid_update() {
  uint32_t now = millis();
  float tmp;
  float alphaP, alphaI, alphaD, alpha;
  bool e;
  
  ESP_LOGVV(TAG, "Entered in pid_update()");
  ESP_LOGVV(TAG, "Current pid mode %d" , this->current_pid_mode_);
  
  
if(this->current_battery_voltage_ < this->current_discharged_battery_voltage_){
	  this->current_epoint_ = this->current_charging_epoint_;
  }
  else if((this->current_battery_voltage_ >= this->current_discharged_battery_voltage_) && (this->current_battery_voltage_ < this->current_charged_battery_voltage_)){
	  this->current_epoint_ = this->current_absorbing_epoint_;
  }
  else{
      this->current_epoint_ = this->current_floating_epoint_;
  }   
  
  e = (this->current_epoint_ < this->current_output_);
  ESP_LOGI(TAG, "previous current_epoint: %2.5f, e: %d" , this->current_epoint_, e );	

#ifdef USE_SWITCH  
  if (!this->current_manual_override_){
#endif
    this->dt_   = float(now - this->last_time_)/1000.0f;
	tmp         = (this->current_input_ - this->current_setpoint_);
	if (e){
		this->error_ = tmp;
	}
	else{
		this->error_ = -tmp;
    }
#ifdef USE_SWITCH	  
	if (this->current_reverse_){
		this->error_ = -this->error_;
	}
#endif	  
	this->current_error_ = this->error_;
	
    tmp = (this->error_ * this->dt_);
    if (!std::isnan(tmp)){
      this->integral_ += tmp;
    }
    this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

    tmp = 0.0f;
    if( !std::isnan(this->previous_output_) && !this->current_pid_mode_){
        tmp = this->previous_output_;
    }
	
	ESP_LOGI(TAG, "previous output = %2.8f" , tmp );
	ESP_LOGI(TAG, "E = %3.2f, I = %3.2f, D = %3.2f, previous = %3.2f" , this->error_ , this->integral_ , this->derivative_ , tmp);
	
	if (e){
	  this->current_kp_ = this->current_kp_charging_;
	  this->current_ki_ = this->current_ki_charging_;
	  this->current_kd_ = this->current_kd_charging_;
	}
	else{
	  this->current_kp_ = this->current_kp_discharging_;
	  this->current_ki_ = this->current_ki_discharging_;
	  this->current_kd_ = this->current_kd_discharging_;
	}
	alphaP = coeffP*this->current_kp_ * this->error_;
	alphaI = coeffI*this->current_ki_ * this->integral_;
	alphaD = coeffD*this->current_kd_ * this->derivative_;
	alpha  = alphaP + alphaI + alphaD;
	
    this->output_ = std::min(std::max( tmp + alpha, this->current_output_min_ ) , this->current_output_max_);
	
    ESP_LOGI(TAG, "Pcoeff = %3.8f" , alphaP );
	ESP_LOGI(TAG, "Icoeff = %3.8f" , alphaI );
	ESP_LOGI(TAG, "Dcoeff = %3.8f" , alphaD );
	
	ESP_LOGI(TAG, "output_min = %1.2f" , this->current_output_min_  );
	ESP_LOGI(TAG, "output_max = %1.2f" , this->current_output_max_  );
	
	ESP_LOGI(TAG, "PIDcoeff = %3.8f" , alpha );
	
  
    ESP_LOGI(TAG, "full pid update: setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_output_ = %3.2f , output_ = %3.2f , error_ = %3.2f, integral = %3.2f , derivative = %3.2f", this->current_target_ , coeffP*this->current_kp_ , coeffI*this->current_ki_ , coeffD*this->current_kd_ , this->current_output_min_ , this->current_output_max_ , this->previous_output_ , this->output_ , this->error_ , this->integral_ , this->derivative_);  

  
    this->last_time_       = now;
    this->previous_error_  = this->error_;
    this->previous_output_ = this->output_;
    
	ESP_LOGI(TAG, "activation %d", this->current_activation_);
	
#ifdef USE_SWITCH  
    if (!this->current_activation_ ){
      this->output_ = 0.5f;
    }
#endif  

    if (!std::isnan(this->current_battery_voltage_)){
	  ESP_LOGI(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f" , this->current_battery_voltage_, this->current_starting_battery_voltage_);	
      if (this->current_battery_voltage_ < this->current_starting_battery_voltage_){
        this->output_ = 0.5f;
      }
    }

	e   = (this->current_epoint_ < this->output_);
	tmp = (this->current_epoint_ - this->output_);
	
	if (e){
	  this->output_charging_ = 2.0f*tmp;
	  this->output_discharging_ = 0.0f;
	}
	else{
	  this->output_charging_ = 0.0f;
	  this->output_discharging_ = 2.0f*tmp;	
	}
	ESP_LOGI(TAG, "Final computed output=%1.6f, output_charging_=%1.6f, output_discharging_=%1.6f" , this->output_, this->output_charging_, this->output_discharging_);
	
    this->device_charging_output_->set_level(this->output_charging_);
	this->device_discharging_output_->set_level(this->output_discharging_);
	this->current_output_ = this->output_;
	this->current_output_charging_ = this->output_charging_;
	this->current_output_discharging_ = this->output_discharging_;  
	
    this->pid_computed_callback_.call();
#ifdef USE_SWITCH	
  } 
#endif  
  
 }

 }  // namespace dualpid
}  // namespace esphome

















