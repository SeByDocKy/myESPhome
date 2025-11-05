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
  
  last_time_ =  millis();
  integral_  = 0.0f;
  previous_output_ = 0.0f;
  previous_error_ = 0.0f;
  
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
    dt_ = float(now - this->last_time_)/1000.0f;
    error_ = -(this->current_setpoint_ - this->current_input_);
	this->current_error_ = error_;
		
    tmp = (error_ * dt_);
    if (!std::isnan(tmp)){
      integral_ += tmp;
    }
    derivative_ = (error_ - previous_error_) / dt_;

    tmp = 0.0f;
    if( !std::isnan(previous_output_) && !this->current_pid_mode_){
        tmp = previous_output_;
    }
	
	ESP_LOGVV(TAG, "previous output = %2.8f" , tmp );
	ESP_LOGVV(TAG, "E = %3.2f, I = %3.2f, D = %3.2f, previous = %3.2f" , error_ , integral_ , derivative_ , tmp);
	
	alphaP = coeffP*this->current_kp_ * error_;
	alphaI = coeffI*this->current_ki_ * integral_;
	alphaD = coeffD*this->current_kd_ * derivative_;
	alpha  = alphaP + alphaI + alphaD;
	
    output_ = std::min(std::max( tmp + alpha, this->current_output_min_  ) , this->current_output_max_);
	
    ESP_LOGVV(TAG, "Pcoeff = %3.8f" , alphaP );
	ESP_LOGVV(TAG, "Icoeff = %3.8f" , alphaI );
	ESP_LOGVV(TAG, "Dcoeff = %3.8f" , alphaD );
	
	ESP_LOGVV(TAG, "output_min = %1.2f" , this->current_output_min_  );
	ESP_LOGVV(TAG, "output_max = %1.2f" , this->current_output_max_  );
	
	ESP_LOGVV(TAG, "PIDcoeff = %3.8f" , alpha );
	
	ESP_LOGVV(TAG, "Intermediate computed output=%1.6f" , output_);
  
    ESP_LOGVV(TAG, "full pid update: setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_output_ = %3.2f , output_ = %3.2f , error_ = %3.2f, integral = %3.2f , derivative = %3.2f", this->current_setpoint_ , coeff*this->current_kp_ , coeff*this->current_ki_ , coeff*this->current_kd_ , this->current_output_min_ , this->current_output_max_ , previous_output_ , output_ , error_ , integral_ , derivative_ );  

  
    last_time_ = now;
    previous_error_ = error_;
    previous_output_ = output_;
    
	ESP_LOGVV(TAG, "activation %d", current_activation_);
	
#ifdef USE_SWITCH  
    if (!this->current_activation_ ){
      output_ = 0.0f;
    }
#endif  

    ESP_LOGVV(TAG, "Final computed output=%1.6f" , output_);
	
    this->device_output_->set_level(output_);
	this->current_output_ = output_;
	
    this->pid_computed_callback_.call();
#ifdef USE_SWITCH	
  } 
#endif  
  
 }

 }  // namespace minipid
}  // namespace esphome



