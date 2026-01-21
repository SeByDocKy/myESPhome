#include "dualpid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpid {

static const char *const TAG = "dualpid";

static const float coeffPcharging = 0.0001f;
static const float coeffIcharging = 0.0001f;
static const float coeffDcharging = 0.001f;

static const float coeffPdischarging = 0.001f;
static const float coeffIdischarging = 0.0001f;
static const float coeffDdischarging = 0.001f;

void DUALPIDComponent::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up DUALPIDComponent...");
  
  this->last_time_ =  millis();
  this->integral_  = 0.0f;
  this->previous_output_ = this->current_epoint_;
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
  float coeffP, coeffI, coeffD;
  float cc, cd;
  bool e;
  
  ESP_LOGI(TAG, "Entered in pid_update()");
  ESP_LOGI(TAG, "Current pid mode %d" , this->current_pid_mode_);
  
  if(this->current_battery_voltage_ < this->current_discharged_battery_voltage_){
	  this->current_epoint_ = this->current_charging_epoint_;
  }
  else if((this->current_battery_voltage_ >= this->current_discharged_battery_voltage_) && (this->current_battery_voltage_ < this->current_charged_battery_voltage_)){
	  this->current_epoint_ = this->current_absorbing_epoint_;
  }
  else{
      this->current_epoint_ = this->current_floating_epoint_;
  }   
  if(this->current_epoint_ != 0.0f){	
    cc = 1.0f/this->current_epoint_;
  }
  else{
	cc = 0.0f;
  }
  cd = 1.0f/(1.0f - this->current_epoint_);
	
  e = (this->current_output_ < this->current_epoint_ ); // test if general regulation point is in charging domain or not
	
  ESP_LOGI(TAG, "previous current_epoint: %2.5f, cc: %2.2f, cd: %2.2f, e: %d" , this->current_epoint_, cc, cd, e );	

#ifdef USE_SWITCH  
  if (!this->current_manual_override_){
#endif
    this->dt_   = float(now - this->last_time_)/1000.0f;
	tmp         = (this->current_input_ - this->current_setpoint_);  // initial epsilon error estimation
	  
	// if (e & tmp<0.0f){
	// 	this->error_ = tmp; //-tmp;
	// }
	// else{
	// 	this->error_ = tmp; 
 //    }
	  
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
      
	  coeffP = coeffPcharging*this->current_kp_;
	  coeffI = coeffIcharging*this->current_kp_;
	  coeffD = coeffDcharging*this->current_kd_;
		
	  alphaP = coeffP * this->error_;
	  alphaI = coeffI * this->integral_;
	  alphaD = coeffD * this->derivative_;
	
	}
	else{
	  this->current_kp_ = this->current_kp_discharging_;
	  this->current_ki_ = this->current_ki_discharging_;
	  this->current_kd_ = this->current_kd_discharging_;

	  coeffP = coeffPdischarging*this->current_kp_;
	  coeffI = coeffIdischarging*this->current_kp_;
	  coeffD = coeffDdischarging*this->current_kd_;	

	  alphaP = coeffP * this->error_;
	  alphaI = coeffI * this->integral_;
	  alphaD = coeffD * this->derivative_;
	}
	
	// alphaP = coeffP*this->current_kp_ * this->error_;
	// alphaI = coeffI*this->current_ki_ * this->integral_;
	// alphaD = coeffD*this->current_kd_ * this->derivative_;
	alpha  = alphaP + alphaI + alphaD;
	
    this->output_ = std::min(std::max( tmp + alpha, this->current_output_min_ ) , this->current_output_max_);
	
    ESP_LOGI(TAG, "Pcoeff = %3.8f" , alphaP );
	ESP_LOGI(TAG, "Icoeff = %3.8f" , alphaI );
	ESP_LOGI(TAG, "Dcoeff = %3.8f" , alphaD );
	
	ESP_LOGI(TAG, "output_min = %1.2f" , this->current_output_min_  );
	ESP_LOGI(TAG, "output_max = %1.2f" , this->current_output_max_  );
	
	ESP_LOGI(TAG, "PIDcoeff = %3.8f" , alpha );
	
	ESP_LOGI(TAG, "full pid update: setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_output_ = %3.2f , output_ = %3.2f , error_ = %3.2f, integral = %3.2f , derivative = %3.2f", this->current_target_ , coeffP*this->current_kp_ , coeffI*this->current_ki_ , coeffD*this->current_kd_ , this->current_output_min_ , this->current_output_max_ , this->previous_output_ , this->output_ , this->error_ , this->integral_ , this->derivative_);    
    // this->last_time_       = now;
    // this->previous_error_  = this->error_;
    // this->previous_output_ = this->output_;
    
	ESP_LOGI(TAG, "activation %d", this->current_activation_);

	e   = (this->output_ < this->current_epoint_ );
	if(e){ // Charge <-> ACin (230V)->R48->DC 48V
       tmp                       = (this->current_epoint_ - this->output_); // tmp is positive
	   this->output_charging_    = tmp; //cc*tmp; ?
	   this->output_discharging_ = 0.0f;	
	   this->output_charging_    = std::min(std::max( this->output_charging_ , this->current_output_min_charging_ ) , this->current_output_max_charging_);
	   this->previous_output_    = this->current_epoint_;
	}
	else{ // Discharge <-> Battery DC 48V->HMS->ACout (230V)
       tmp                       = (this->output_ - this->current_epoint_ ); // tmp is positive
	   this->output_charging_    = 0.0f;
	   this->output_discharging_ = cd*tmp; // tmp;?
	   this->output_discharging_ = std::min(std::max( this->output_discharging_ , this->current_output_min_discharging_ ) , this->current_output_max_discharging_);	
	   this->previous_output_    = this->current_epoint_;
	}
	// tmp is a positive value

	// if (e){
	//   this->output_charging_    = cc*tmp; //0.0f;    //;
	//   this->output_discharging_ = 0.0f; //0.0f;
	// }
	// else{
	//   this->output_charging_    = 0.0f; //0.0f;
	//   this->output_discharging_ = cd*tmp;   // cd*tmp;	
	// }
  
#ifdef USE_SWITCH  
    if (!this->current_activation_ ){
      this->output_             = this->current_epoint_;
	  this->previous_output_    = this->current_epoint_;  	
	  this->output_charging_    = 0.0f;
	  this->output_discharging_ = 0.0f;	
    }
#endif  
// #ifdef USE_SWITCH 
//    if((this->output_charging_ > 0.0f) & (this->get_r48()==false)){
//       this->set_r48(true);
//    }
//    else if ((this->output_discharging_ > 0.0f) & (this->get_r48()==true)){
//       this->set_r48(false);
//    } 
// #endif
	  
    if (this->r48_general_switch_ != nullptr) {
 	 if((this->output_charging_ >= this->current_output_min_charging_) & (this->r48_general_switch_->state==false)){
       // this->r48_general_switch_->control(true);
	   this->r48_general_switch_->turn_on();	 
	   this->r48_general_switch_->publish_state(true);	
     }
	 else if  ((this->output_discharging_ >= this->current_output_min_discharging_) & (this->r48_general_switch_->state==true)){
       // this->r48_general_switch_->control(false);
	   this->r48_general_switch_->turn_off();	 
	   this->r48_general_switch_->publish_state(false);
	 }
    }

    if (!std::isnan(this->current_battery_voltage_)){
	  ESP_LOGI(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f" , this->current_battery_voltage_, this->current_starting_battery_voltage_);	
      if (this->current_battery_voltage_ < this->current_starting_battery_voltage_){
        this->output_             = this->current_epoint_;
		this->previous_output_    = this->current_epoint_;   
		this->output_charging_    = 0.0f;
	    this->output_discharging_ = 0.0f;  
      }
    }


	ESP_LOGI(TAG, "Final computed output=%1.6f, output_charging_=%1.6f, output_discharging_=%1.6f" , this->output_, this->output_charging_, this->output_discharging_);
	if (this->output_charging_ != this->previous_output_charging_){
      this->device_charging_output_->set_level(this->output_charging_);          // send command to r48, must be in [0.0 - 1.0] //
	}
	if (this->output_discharging_ != this->previous_output_discharging_){  
	  this->device_discharging_output_->set_level(this->output_discharging_);    // send command to HMS, must be in [0.0 - 1.0] //
	}
	this->current_output_             = this->output_;  // must be in [0.0 - 1.0] //
	this->current_output_charging_    = this->output_charging_;
	  
#ifdef USE_BINARY_SENSOR
	if (this->producing_binary_sensor_ != nullptr & this->producing_binary_sensor_->state==true){ // control HMS only when it's starting to produce if not... HMS is blocked
#endif		
	this->current_output_discharging_ = this->output_discharging_;  
#ifdef USE_BINARY_SENSOR
	}
#endif
    this->pid_computed_callback_.call();

    this->last_time_                   = now;
    this->previous_error_              = this->error_;
    this->previous_output_             = this->output_;
	this->previous_output_charging_    = this->output_charging_;
	this->previous_output_discharging_ = this->output_discharging_;
	  
	  
#ifdef USE_SWITCH	
  } 
#endif  
  
 }

 }  // namespace dualpid
}  // namespace esphome
