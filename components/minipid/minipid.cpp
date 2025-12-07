#include "minipid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace minipid {

static const char *const TAG = "minipid";
static const float coeffP = 0.001f;
static const float coeffI = 0.0001f;
static const float coeffD = 0.001f;

void MINIPIDComponent::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up MINIPIDComponent...");
  
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
  
  this->pid_computed_callback_.call();
  // this->pid_update();
  
  ESP_LOGVV(TAG, "setup: input=%3.2f, pid_mode = %d", this->current_input_ , this->current_pid_mode_);  
}

void MINIPIDComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  ESP_LOGVV(TAG, "setup import part: input=%3.2f", this->current_input_);
  this->pid_computed_callback_.call(); 
}

void MINIPIDComponent::pid_update() {
  uint32_t now = millis();
  float tmp;
  float alphaP, alphaI, alphaD, alpha;
  
  ESP_LOGVV(TAG, "Entered in pid_update()");
  ESP_LOGVV(TAG, "Current pid mode %d" , this->current_pid_mode_);
    
#ifdef USE_SWITCH  
  if (!this->current_manual_override_){
#endif
    this->dt_ = float(now - this->last_time_)/1000.0f;
    this->error_ = -(this->current_setpoint_ - this->current_input_);
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
	
	ESP_LOGVV(TAG, "previous output = %2.8f" , tmp );
	ESP_LOGVV(TAG, "E = %3.2f, I = %3.2f, D = %3.2f, previous = %3.2f" , this->error_ , this->integral_ , this->derivative_ , tmp);
	
	alphaP = coeffP*this->current_kp_ * this->error_;
	alphaI = coeffI*this->current_ki_ * this->integral_;
	alphaD = coeffD*this->current_kd_ * this->derivative_;
	alpha  = alphaP + alphaI + alphaD;
	
    this->output_ = std::min(std::max( tmp + alpha, this->current_output_min_  ) , this->current_output_max_);
	
    ESP_LOGVV(TAG, "Pcoeff = %3.8f" , alphaP );
	ESP_LOGVV(TAG, "Icoeff = %3.8f" , alphaI );
	ESP_LOGVV(TAG, "Dcoeff = %3.8f" , alphaD );
	
	ESP_LOGVV(TAG, "output_min = %1.2f" , this->current_output_min_  );
	ESP_LOGVV(TAG, "output_max = %1.2f" , this->current_output_max_  );
	
	ESP_LOGVV(TAG, "PIDcoeff = %3.8f" , alpha );
	
	ESP_LOGVV(TAG, "Intermediate computed output=%1.6f" , this->output_);
  
    ESP_LOGVV(TAG, "full pid update: setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_output_ = %3.2f , output_ = %3.2f , error_ = %3.2f, integral = %3.2f , derivative = %3.2f", this->current_setpoint_ , coeffP*this->current_kp_ , coeffI*this->current_ki_ , coeffD*this->current_kd_ , this->current_output_min_ , this->current_output_max_ , this->previous_output_ , this->output_ , this->error_ , this->integral_ , this->derivative_ );  

  
    // this->last_time_ = now;
    // this->previous_error_ = this->error_;
    // this->previous_output_ = this->output_;
    
	ESP_LOGVV(TAG, "activation %d", this->current_activation_);
	
#ifdef USE_SWITCH  
    if (!this->current_activation_ ){
      this->output_ = 0.0f;
    }
#endif  

    ESP_LOGVV(TAG, "Final computed output=%1.6f" , this->output_);

	/// Output must be in [0.0 - 1.0] ////

	if (this->output_ != this->previous_output_){
      this->device_output_->set_level(this->output_);
	}
	this->current_output_ = this->output_;
    this->pid_computed_callback_.call();

	this->last_time_       = now;
    this->previous_error_  = this->error_;
    this->previous_output_ = this->output_;  
	  
#ifdef USE_SWITCH	
  } 
#endif  
  
 }

 }  // namespace minipid
}  // namespace esphome









