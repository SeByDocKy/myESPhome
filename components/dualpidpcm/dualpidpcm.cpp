#include "dualpidpcm.h"
#include "esphome/core/log.h"

#define SET_OUTPUT_DELAY 0            // 50
#define ONOFF_DELAY 0                 // 50
#define CHARGE_DISCHARGE_DELAY 0      // 50

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
	this->previous_output_charging_= 0.0f;
	this->previous_output_discharging_= 0.0f;  
  
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
    float cc=1.0f/(this->epoint_ - this->elb_), cd=1.0f/(1.0f - this->epoint_ - this->eub_);
    bool current_state=true, previous_state=true;
	int offcharge = 0, offdischarge = 0;
  
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

	  tmp = 0.0f;
      if( !std::isnan(this->previous_output_) && !this->current_pid_mode_){
        tmp = this->previous_output_;
      }
	
	  ESP_LOGI(TAG, "previous output = %2.8f" , tmp );
	  ESP_LOGI(TAG, "E = %3.2f, I = %3.2f, D = %3.2f, previous = %3.2f" , this->error_ , this->integral_ , this->derivative_ , tmp);
	

	  if (previous_state != current_state){
	    this->current_swap_ = true; //swap_state = true; 
      }
	  else{
        this->current_swap_ = false; //swap_state = false;
	  }



      if ((this->current_output_ <= this->epoint_) & (offcharge >= 0) & (offcharge < 3) & (offdischarge == 0)){  // charge   offcharge

	    this->current_kp_ = this->current_kp_charging_;
	    this->current_ki_ = this->current_ki_charging_;
	    this->current_kd_ = this->current_kd_charging_;
      
	    coeffP            = coeffPcharging*this->current_kp_;
	    coeffI            = coeffIcharging*this->current_ki_;
	    coeffD            = coeffDcharging*this->current_kd_;
		
	    alphaP            = coeffP * this->error_;
	    alphaI            = coeffI * this->integral_;
	    alphaD            = coeffD * this->derivative_;

	    // this->current_deadband_   = false;

		alpha                     = alphaP + alphaI + alphaD;
	    this->current_output_     = std::min(std::max( tmp + alpha, this->output_min_ ) , this->output_max_);
		if((this->current_output_ >  this->epoint_) & (this->current_output_ < this->epoint_ + this->eub_) ){
			offcharge++;
			this->current_deadband_   = true;
		}
		else{
            offcharge = 0;
			this->current_deadband_   = false;
		}

		tmp                       = (this->epoint_  - this->current_output_); // - this->elb_     tmp is positive
	    this->output_charging_    = cc*tmp; //cc*tmp; ?
	    this->output_discharging_ = 0.0f;	
	    this->output_charging_    = std::min(std::max( this->output_charging_ , this->current_output_min_charging_ ) , this->current_output_max_charging_);
	    // this->previous_output_    = this->current_epoint_;  
  
	  }
	  if ((this->current_output_ > this->epoint_) & (offdischarge >= 0) & (offdischarge < 3) & (offcharge == 0)) {// if (this->current_output_ > this->epoint_ + this->eub_){ //discharge
	    this->current_kp_ = this->current_kp_discharging_;
	    this->current_ki_ = this->current_ki_discharging_;
	    this->current_kd_ = this->current_kd_discharging_;
		 
	    coeffP            = coeffPdischarging*this->current_kp_;
	    coeffI            = coeffIdischarging*this->current_ki_;
	    coeffD            = coeffDdischarging*this->current_kd_;	

	    alphaP            = coeffP * this->error_;
	    alphaI            = coeffI * this->integral_;
	    alphaD            = coeffD * this->derivative_;

	    //this->current_deadband_   = false;
		
		alpha                     = alphaP + alphaI + alphaD;
	    this->current_output_     = std::min(std::max( tmp + alpha, this->output_min_ ) , this->output_max_);
		if((this->current_output_ <  this->epoint_) & (this->current_output_ > this->epoint_ - this->elb_) ){
			offdischarge++;
			this->current_deadband_   = true;
		}
		else{
            offdischarge = 0;
			this->current_deadband_   = false;
		}  

		tmp                       = (this->current_output_  - this->epoint_ ); // + this->eub_       tmp is positive
	    this->output_charging_    = 0.0f;
	    this->output_discharging_ = cd*tmp; // tmp;?
	    this->output_discharging_ = std::min(std::max( this->output_discharging_ , this->current_output_min_discharging_ ) , this->current_output_max_discharging_);	
	    // this->previous_output_    = this->current_epoint_;
	  }
	  // else{ // deadband
    //     if((epsi > -this->current_battery_voltage_*this->current_min_charging_) & (epsi < this->current_battery_voltage_*this->current_min_discharging_)){
		  // previous_state            = current_state;
	   //    this->current_deadband_   = true;
			
		  // // this->current_output_     = 0.5f;	
		  // // this->output_charging_    = 0.0f;
		  // // this->output_discharging_ = 0.0f;

		  // this->current_output_     = this->previous_output_;	
		  // this->output_charging_    = this->previous_output_charging_;
		  // this->output_discharging_ = this->previous_output_discharging_;
	
	
	   //  }
      // }
		
	  // alpha  = alphaP + alphaI + alphaD;
	  // this->output_ = std::min(std::max( tmp + alpha, this->current_output_min_ ) , this->current_output_max_);
		





		
	 //  this->dt_    = float(now - this->last_time_)/1000.0f;
	 //  epsi         = (this->current_input_ - this->current_setpoint_);  // initial epsilon error estimation
	
	 //  if(epsi < -this->current_battery_voltage_*this->current_min_charging_){  // charge battery
	 //    this->error_ = - epsi;   
	  	  
	 //    if (this->current_reverse_){
		//   this->error_ = -this->error_;
	 //    }
	 //    this->current_error_ = this->error_;
	
  //       tmp = (this->error_ * this->dt_);
  //       if (!std::isnan(tmp)){
  //         this->integral_ += tmp;
  //       }
  //       this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

	 //    if (previous_state != current_state){
	 //      this->current_swap_ = true; //swap_state = true; 
  //       }
	 //    else{
  //         this->current_swap_ = false; //swap_state = false;
	 //    }

	 //    tmp = 0.0f;
  //       if( !std::isnan(this->previous_output_charging_) && !this->current_pid_mode_   ){ //  && !swap_state  
  //         tmp = this->previous_output_charging_;
  //       }
	      
	 //    coeffP = coeffPcharging*this->current_kp_charging_;
	 //    coeffI = coeffIcharging*this->current_ki_charging_;
	 //    coeffD = coeffDcharging*this->current_kd_charging_;
		
	 //    alphaP = coeffP * this->error_;
	 //    alphaI = coeffI * this->integral_;
	 //    alphaD = coeffD * this->derivative_;

	 //    alpha  = alphaP + alphaI + alphaD;	
	 //    this->current_deadband_ = false;
	 //    previous_state = current_state;	
	 //    current_state = false;

	 //    this->output_discharging_ = 0.0f;		
	 //    this->output_charging_    = std::min(std::max( tmp + alpha , this->current_output_min_charging_ ) , this->current_output_max_charging_);	
	 //    ESP_LOGI(TAG, "Charge battery,  output_charging_=%1.6f, output_discharging_=%1.6f" , this->output_charging_, this->output_discharging_); 
	 //  }
	 //  else if (epsi > this->current_battery_voltage_*this->current_min_discharging_){  // discharge battery

	 //    this->error_ = epsi;   
	  	  
	 //    if (this->current_reverse_){
		//   this->error_ = -this->error_;
	 //    }
	 //    this->current_error_ = this->error_;
	
  //       tmp = (this->error_ * this->dt_);
  //       if (!std::isnan(tmp)){
  //         this->integral_ += tmp;
  //       }
  //       this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

	 //    if (previous_state != current_state){
	 //      this->current_swap_ = true; //swap_state = true; 
  //       }
	 //    else{
  //         this->current_swap_ = false; //swap_state = false;
	 //    }
  
	 //    tmp = 0.0f;
  //       if( !std::isnan(this->previous_output_discharging_) && !this->current_pid_mode_    ){ //   && !swap_state
  //         tmp = this->previous_output_discharging_;
  //       }	
        		  
	 //    coeffP = coeffPdischarging*this->current_kp_discharging_;
	 //    coeffI = coeffIdischarging*this->current_ki_discharging_;
	 //    coeffD = coeffDdischarging*this->current_kd_discharging_;	

	 //    alphaP = coeffP * this->error_;
	 //    alphaI = coeffI * this->integral_;
	 //    alphaD = coeffD * this->derivative_;

	 //    alpha  = alphaP + alphaI + alphaD;	
	 //    this->current_deadband_ = false;
	 //    previous_state = current_state;	
	 //    current_state = true;	

	 //    this->output_charging_    = 0.0f;		
	 //    this->output_discharging_ = std::min(std::max( tmp + alpha , this->current_output_min_discharging_ ) , this->current_output_max_discharging_);	
	 // 	ESP_LOGI(TAG, "Discharge battery,  output_charging_=%1.6f, output_discharging_=%1.6f" , this->output_charging_, this->output_discharging_);
	 //  }
	 //  else{  // deadband

		// this->error_ = epsi;   
	 //  	this->current_error_ = this->error_;
		  
	 //    alphaP = 0.0f;
		// alphaI = 0.0f;
		// alphaD = 0.0f;
		// previous_state = current_state;
	 //    this->current_deadband_ = true;
		// this->output_charging_ = 0.0f;
		// this->output_discharging_ = 0.0f;
	 //  }
		
      if (!this->current_activation_ ){  // no regulation 
	    this->output_charging_    = 0.0f;
	    this->output_discharging_ = 0.0f;
		this->previous_output_    = 0.5f;
		this->current_output_     = 0.5f;	  
	    if((this->onoff_switch_->state == true)  ){
		  this->onoff_switch_->turn_off();
		  this->onoff_switch_->publish_state(false);	
		  delay(ONOFF_DELAY);
			
		  this->discharge_charge_switch_->turn_on();
		  this->discharge_charge_switch_->publish_state(true);	
		  delay(CHARGE_DISCHARGE_DELAY);
		  ESP_LOGI(TAG, "activation is off -> Turn off onoff, turn on discharge_charge");	
        }	
      }		
	  else{  // regulation
	    if (!this->current_deadband_){ // Not in deadband
          if (this->discharge_charge_switch_ != nullptr) {
 	        if((this->output_charging_ > this->current_output_min_charging_) & (this->discharge_charge_switch_->state==false)){
			  this->discharge_charge_switch_->turn_on();
			  this->discharge_charge_switch_->publish_state(true);
		      delay(ONOFF_DELAY);
			  ESP_LOGI(TAG, "Turn on discharge_charge");	
            }
	        else if  ((this->output_discharging_ > this->current_output_min_discharging_) & (this->discharge_charge_switch_->state==true)){
			  this->discharge_charge_switch_->turn_off();
			  this->discharge_charge_switch_->publish_state(false);	
		      delay(CHARGE_DISCHARGE_DELAY);
			  ESP_LOGI(TAG, "Turn off discharge_charge");	
	        }
          }
	    }
	    else{ // in deadband, turn off PCM module
          if((this->onoff_switch_->state == true)  ){
	         this->onoff_switch_->turn_off();
			 this->onoff_switch_->publish_state(false); 
		     delay(ONOFF_DELAY);
			 ESP_LOGI(TAG, "in deadband -> Turn off onoff"); 
          }
	    }
	  }

      if (!std::isnan(this->current_battery_voltage_)){
	    ESP_LOGI(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f" , this->current_battery_voltage_, this->current_starting_battery_voltage_);	
        if (this->current_battery_voltage_ < this->current_starting_battery_voltage_){
		  this->output_charging_    = 0.0f;
	      this->output_discharging_ = 0.0f;
		  this->previous_output_    = 0.5f;
		  this->current_output_     = 0.5f;	

		  this->onoff_switch_->publish_state(false);	
          this->onoff_switch_->turn_off();
		  delay(ONOFF_DELAY);	
	      
          this->discharge_charge_switch_->publish_state(true);			  
          this->discharge_charge_switch_->turn_on();
		  delay(CHARGE_DISCHARGE_DELAY);	
        }
      }

	
	  ESP_LOGI(TAG, "Final computed output_charging_=%1.6f, output_discharging_=%1.6f" , this->output_charging_, this->output_discharging_);  

	  if ((this->output_charging_ != this->previous_output_charging_) & (this->onoff_switch_->state==true) & (offcharge==0) ){
        this->device_charging_output_->set_level(this->output_charging_);          // send command to PCM must be in [0.0 - 1.0] //
	    delay(SET_OUTPUT_DELAY);
	  }
	  if ((this->output_discharging_ != this->previous_output_discharging_) & (this->onoff_switch_->state==true) & (offdischarge==0)){  
	    this->device_discharging_output_->set_level(this->output_discharging_);    // send command to PCM, must be in [0.0 - 1.0] //
        delay(SET_OUTPUT_DELAY);
	  }
	  this->current_output_charging_    = this->output_charging_;
	  this->current_output_discharging_ = this->output_discharging_;  

      this->last_time_                   = now;
      this->previous_error_              = this->error_;
	  this->previous_output_             = this->current_output_;	
	  this->previous_output_charging_    = this->output_charging_;
	  this->previous_output_discharging_ = this->output_discharging_;

	  if (this->current_activation_ ){  
	    if (this->onoff_switch_ != nullptr){
		  if((this->onoff_switch_->state==false) & ((this->output_charging_ > 0.0f) | (this->output_discharging_ > 0.0f))){
		    this->onoff_switch_->turn_on();	 
	        this->onoff_switch_->publish_state(true);
			delay(ONOFF_DELAY);  
			// ESP_LOGI(TAG, "Turn on on off");  
		  }
	    }
      }

	  this->pid_computed_callback_.call();
		
    } 
   } // pid_update

 }  // namespace dualpidpcm
}  // namespace esphome
