#include "dualpidpcm.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpidpcm {

  static const char *const TAG = "dualpidpcm";

  static const float coeffPcharging = 0.00001f;
  static const float coeffIcharging = 0.001f;
  static const float coeffDcharging = 0.001f;

  static const float coeffPdischarging = 0.00001f;
  static const float coeffIdischarging = 0.001f;
  static const float coeffDdischarging = 0.001f;

  void DUALPIDPCMComponent::setup() { 
    ESP_LOGCONFIG(TAG, "Setting up DUALPIDPCMComponent...");
  
    this->last_time_      =  millis();
    this->integral_       = 0.0f;
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
      });
      this->current_battery_voltage_ = this->battery_voltage_sensor_->state;
    }
  
    this->pid_computed_callback_.call();

    ESP_LOGI(TAG, "setup: battery_voltage=%3.2f, pid_mode = %d", this->current_battery_voltage_ , this->current_pid_mode_);  
  
  }

  void DUALPIDPCMComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "dump config:");
    ESP_LOGVV(TAG, "setup import part: battery_voltage=%3.2f", this->current_battery_voltage_ );
    this->pid_computed_callback_.call(); 
  }

  void DUALPIDPCMComponent::pid_update() {
    uint32_t now = millis();
    float tmp, epsi;
    float alphaP, alphaI, alphaD;
	float alpha;
    float coeffP, coeffI, coeffD;
    float cc, cd;
    bool e ;
    bool current_state=true, previous_state=true;
	// bool swap_state=false;
  
    ESP_LOGI(TAG, "Entered in pid_update()");
    ESP_LOGI(TAG, "Current pid mode %d" , this->current_pid_mode_);

    if (!this->current_manual_override_){
      this->dt_    = float(now - this->last_time_)/1000.0f;
	  epsi         = (this->current_input_ - this->current_setpoint_);  // initial epsilon error estimation
	  this->error_ = epsi;   
	  	  
	  if (this->current_reverse_){
		this->error_ = -this->error_;
	  }
	  this->current_error_ = this->error_;
	
      tmp = (this->error_ * this->dt_);
      if (!std::isnan(tmp)){
        this->integral_ += tmp;
      }
      this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

	  if (previous_state != current_state){
	    this->current_swap_ = true; //swap_state = true; 
      }
	  else{
        this->current_swap_ = false; //swap_state = false;
	  }


		
	  if(epsi < -this->current_battery_voltage_*this->current_min_charging_){  // charge battery

	    tmp = 0.0f;
        if( !std::isnan(this->previous_output_charging_) && !this->current_pid_mode_   ){ //  && !swap_state  
          tmp = this->previous_output_charging_;
        }
	      
	    coeffP = coeffPcharging*this->current_kp_charging_;
	    coeffI = coeffIcharging*this->current_ki_charging_;
	    coeffD = coeffDcharging*this->current_kd_charging_;
		
	    alphaP = coeffP * this->error_;
	    alphaI = coeffI * this->integral_;
	    alphaD = coeffD * this->derivative_;

	    alpha  = alphaP + alphaI + alphaD;	
	    this->current_deadband_ = false;
	    previous_state = current_state;	
	    current_state = false;

	    this->output_discharging_ = 0.0f;		
	    this->output_charging_    = std::min(std::max( tmp + alpha , this->current_output_min_charging_ ) , this->current_output_max_charging_);	
	  
	  }
	  else if (epsi > this->current_battery_voltage_*this->current_min_discharging_){  // discharge battery

	    tmp = 0.0f;
        if( !std::isnan(this->previous_output_discharging_) && !this->current_pid_mode_    ){ //   && !swap_state
          tmp = this->previous_output_discharging_;
        }	
        		  
	    coeffP = coeffPdischarging*this->current_kp_discharging_;
	    coeffI = coeffIdischarging*this->current_ki_discharging_;
	    coeffD = coeffDdischarging*this->current_kd_discharging_;	

	    alphaP = coeffP * this->error_;
	    alphaI = coeffI * this->integral_;
	    alphaD = coeffD * this->derivative_;

	    alpha  = alphaP + alphaI + alphaD;	
	    this->current_deadband_ = false;
	    previous_state = current_state;	
	    current_state = true;	

	    this->output_charging_    = 0.0f;		
	    this->output_discharging_ = std::min(std::max( tmp + alpha , this->current_output_min_discharging_ ) , this->current_output_max_discharging_);	
	 	
	  }
	  else{  // deadband
	    alphaP = 0.0f;
		alphaI = 0.0f;
		alphaD = 0.0f;
		// alpha  = 0.5f;
		previous_state = current_state;
	    this->current_deadband_ = true;
		this->output_charging_ = 0.0f;
		this->output_discharging_ = 0.0f;
	  }
		
 
      if (!this->current_activation_ ){  // no regulation 
	    this->output_charging_    = 0.0f;
	    this->output_discharging_ = 0.0f;	
	    if((this->onoff_switch_->state==true)  ){
	      this->onoff_switch_->publish_state(false);
		  this->onoff_switch_->turn_off();	 
		  delay(300);
			
          this->discharge_charge_switch_->publish_state(true);		  
		  this->discharge_charge_switch_->turn_on();	 
		  delay(300);
        }	
      }
	  else{  // regulation
	    if (!this->current_deadband_){ // Not in deadband
          if (this->discharge_charge_switch_ != nullptr) {
 	        if((this->output_charging_ > this->current_output_min_charging_) & (this->discharge_charge_switch_->state==false)){
              this->discharge_charge_switch_->publish_state(true);
			  this->discharge_charge_switch_->turn_on();
		      delay(300);
            }
	        else if  ((this->output_discharging_ > this->current_output_min_discharging_) & (this->discharge_charge_switch_->state==true)){
              this->discharge_charge_switch_->publish_state(false);
			  this->discharge_charge_switch_->turn_off();	 
		      delay(300);
	        }
          }
	    }
	    else{ // in deadband, turn off PCM module
          if((this->onoff_switch_->state==true)  ){
			 this->onoff_switch_->publish_state(false); 
	         this->onoff_switch_->turn_off();
		     delay(300);
          }
	    }
	  }

      if (!std::isnan(this->current_battery_voltage_)){
	    ESP_LOGI(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f" , this->current_battery_voltage_, this->current_starting_battery_voltage_);	
        if (this->current_battery_voltage_ < this->current_starting_battery_voltage_){
		  this->output_charging_    = 0.0f;
	      this->output_discharging_ = 0.0f;

		  this->onoff_switch_->publish_state(false);	
          this->onoff_switch_->turn_off();
		  delay(300);	
	      
          this->discharge_charge_switch_->publish_state(true);			  
          this->discharge_charge_switch_->turn_on();
		  delay(300);	
        }
      }
	  // // this->pid_computed_callback_.call();	
	
	  ESP_LOGI(TAG, "Final computed output_charging_=%1.6f, output_discharging_=%1.6f" , this->output_charging_, this->output_discharging_);  
	  if (this->output_charging_ != this->previous_output_charging_){
        this->device_charging_output_->set_level(this->output_charging_);          // send command to r48, must be in [0.0 - 1.0] //
	    delay(150);
	  }
	  if (this->output_discharging_ != this->previous_output_discharging_){  
	    this->device_discharging_output_->set_level(this->output_discharging_);    // send command to HMS, must be in [0.0 - 1.0] //
        delay(150);
	  }
	  this->current_output_charging_    = this->output_charging_;
	  this->current_output_discharging_ = this->output_discharging_;  

      this->last_time_                   = now;
      this->previous_error_              = this->error_;
	  this->previous_output_charging_    = this->output_charging_;
	  this->previous_output_discharging_ = this->output_discharging_;

	  if (this->current_activation_ ){  
	    if (this->onoff_switch_ != nullptr){
		  if((this->onoff_switch_->state==false) & ((this->output_charging_ > 0.0f) | (this->output_discharging_ > 0.0f))){
		    this->onoff_switch_->turn_on();	 
	        this->onoff_switch_->publish_state(true);
			delay(300);  
		  }
	    }
      }

	  this->pid_computed_callback_.call();
		
    } 
   } // pid_update

 }  // namespace dualpidpcm
}  // namespace esphome
