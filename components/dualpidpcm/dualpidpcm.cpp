#include "dualpidpcm.h"
#include "esphome/core/log.h"

#define SET_OUTPUT_DELAY 0            // 50
#define ONOFF_DELAY 0                 // 50
#define CHARGE_DISCHARGE_DELAY 0      // 50
#define DEADBAND_FACTOR 1.05


namespace esphome {
namespace dualpidpcm {

  static const char *const TAG = "dualpidpcm";

  static const float coeffP = 0.00001f;
  static const float coeffI = 0.001f;
  static const float coeffD = 0.001f;

  float DUALPIDPCMComponent::O_to_Oc(float O) {
	 if (O > this->oneutral_) return 0.0f;
     return 1.0f - (O / this->oneutral_);   // linéaire inversé
  }
  float DUALPIDPCMComponent::O_to_Od(float O) {
	if (O < this->oneutral_) return 0.0f;
    return (O - this->oneutral_) / (1.0f - this->oneutral_);
  }
  
  void DUALPIDPCMComponent::setup() { 
    ESP_LOGCONFIG(TAG, "Setting up DUALPIDPCMComponent...");
  
    this->last_time_                   =  millis();
    this->integral_                    = 0.0f;
    this->previous_error_              = 0.0f;
	this->previous_output_charging_    = 0.0f;
	this->previous_output_discharging_ = 0.0f;
	this->previous_activation_         = false;
	this->previous_mode_               = 0;  
	this->current_mode_                = 0;
  
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

	if (this->onoff_switch_ != nullptr){
		  if((this->onoff_switch_->state==true) ){
		    this->onoff_switch_->turn_off();	 
	        this->onoff_switch_->publish_state(false);  
		  }
	}


	this->olb_  = this->oneutral_ - this->lb_;
	this->oub_  = this->oneutral_ + this->ub_;  
	  
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
    float tmp, tmp_i, epsi;
    float alphaP, alphaI, alphaD;
	float alpha;
	bool should_be_on, raw_deadband, output_is_active;
	float o_min_charge, o_max_charge, o_min_discharge, o_max_discharge, o_clamped;
    
    ESP_LOGI(TAG, "Entered in pid_update()");
    ESP_LOGI(TAG, "Current pid mode %d" , this->current_pid_mode_);

	this->pid_computed_callback_.call();  

	
    if (!this->current_manual_override_){
      this->dt_                           = float(now - this->last_time_)/1000.0f;
	  if (this->dt_ < 0.001f) {
       this->last_time_                   = now;   // on avance quand même l'horloge
	   this->previous_output_charging_    = this->current_output_charging_;
       this->previous_output_discharging_ = this->current_output_discharging_;	  
       return;
      } 	
	  epsi         = (this->current_input_ - this->current_setpoint_);  // initial epsilon error estimation
	  this->error_ = epsi;   
	  	  
	  if (this->current_reverse_){
		this->error_ = -this->error_;
	  }
	  this->current_error_ = this->error_; 


	  if (this->current_activation_ && !this->previous_activation_) {
       this->previous_output_ = this->oneutral_;
       this->current_output_  = this->oneutral_;
       this->previous_error_  = this->error_;  // bonus : évite spike dérivé si Kd > 0 un jour
       this->integral_        = 0.0f;          // repart proprement
      }
      this->previous_activation_ = this->current_activation_;	

	  this->Pmin_charging      = - this->current_battery_voltage_*this->current_min_charging_;
	  this->Pmin_discharging   =   this->current_battery_voltage_*this->current_min_discharging_;
	  

      raw_deadband     = (epsi > this->Pmin_charging  * DEADBAND_FACTOR) && (epsi < this->Pmin_discharging * DEADBAND_FACTOR);
      output_is_active = (this->current_output_charging_  > this->current_output_min_charging_) || (this->current_output_discharging_  > this->current_output_min_discharging_);
      this->current_deadband_ = raw_deadband && !output_is_active;
		
      this->offcharge_  = this->previous_mode_;
		
	  if ((this->current_deadband_) && (this->previous_mode_ == 0)) {
        // Rien à faire, on reste off
		if (this->onoff_switch_ != nullptr){
		  if((this->onoff_switch_->state==true) ){
		    this->onoff_switch_->turn_off();	 
	        this->onoff_switch_->publish_state(false);  
		  }
	    }  
    
		this->current_output_charging_     = 0.0f;	
	    this->current_output_discharging_  = 0.0f;
		this->last_time_                   = now;
        this->previous_error_              = this->error_;  
		this->previous_output_charging_    = this->current_output_charging_;
        this->previous_output_discharging_ = this->current_output_discharging_;  
        return;
      }	
	  // // ── Entrée en deadband depuis un mode ACTIF : arrêt immédiat ──────
		
      if (this->current_deadband_ && (this->previous_mode_ != 0)) {
        if (this->onoff_switch_ != nullptr && this->onoff_switch_->state == true) {
          this->onoff_switch_->turn_off();
          this->onoff_switch_->publish_state(false);
          delay(ONOFF_DELAY);
        }
       this->current_output_charging_    = 0.0f;
       this->current_output_discharging_ = 0.0f;
       this->current_onoff_              = false;
      // Repartir à la frontière du mode qu'on vient de quitter
      // pour que le PID franchisse le seuil dès le 1er cycle
       if (this->previous_mode_ == 2) {
          this->previous_output_ = this->oub_;
          this->current_output_  = this->oub_;
       } 
	  else if (this->previous_mode_ == 1) {
        this->previous_output_ = this->olb_;
        this->current_output_  = this->olb_;
      }

      this->previous_mode_ = 0;
      this->current_mode_  = 0;
      this->last_time_     = now;
      this->previous_error_ = this->error_;
      return;
     }		
		
	  tmp_i = (this->error_ * this->dt_);
      if (!std::isnan(tmp_i)){
        this->integral_ += tmp_i;
      }
      this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

	  tmp = 0.0f;
      if( !std::isnan(this->previous_output_) && (!this->current_pid_mode_)){
        tmp = this->previous_output_;
      }	
		
	  alphaP                    = coeffP * this->current_kp_ * this->error_;
	  alphaI                    = coeffI * this->current_ki_ * this->integral_;
	  alphaD                    = coeffD * this->current_kd_ * this->derivative_;
	  alpha                     = alphaP + alphaI + alphaD;	

	  this->current_output_     = std::min(std::max( tmp + alpha, this->output_min_ ) , this->output_max_);
		
      // ── Nouveau : clamping de O selon le mode courant ──────────────
     if (this->previous_mode_ == 1) {        // CHARGE
       o_min_charge = (1.0f - this->current_output_max_charging_) * this->oneutral_;
       o_max_charge = (1.0f - this->current_output_min_charging_) * this->oneutral_;
       o_clamped    = std::min(std::max(this->current_output_, o_min_charge), o_max_charge);
       if (o_clamped != this->current_output_) {
         this->integral_ -= tmp_i;   // anti-windup : on était contre une borne
       }
       this->current_output_ = o_clamped;
     } 
	 else if (this->previous_mode_ == 2) { // DISCHARGE
       o_min_discharge = this->current_output_min_discharging_ * this->oneutral_ + this->oneutral_;
       o_max_discharge = this->current_output_max_discharging_ * this->oneutral_ + this->oneutral_;
       o_clamped       = std::min(std::max(this->current_output_, o_min_discharge), o_max_discharge);
       if (o_clamped != this->current_output_) {
        this->integral_ -= tmp_i;   // anti-windup
       }
       this->current_output_ = o_clamped;
     }
		
	  this->current_mode_	   = this->previous_mode_;		
	  switch (this->previous_mode_) {

        case 0:
            if (this->current_output_ < this->olb_)
                this->current_mode_ = 1;
            else if (this->current_output_ > this->oub_)
                this->current_mode_ = 2;
            // sinon on reste IDLE
            break;

        case 1:
            // On quitte la charge seulement si O monte au-delà
            // de O_hi (pas juste au-dessus de 0.5)
            if (this->current_output_ > this->oub_)
                this->current_mode_ = 2;
            else if ( (this->current_output_ >= this->olb_) && (this->current_output_ <= this->oub_) && (this->current_deadband_))
                this->current_mode_ = 0;
            break;

        case 2:
            // Idem, on quitte la décharge seulement sous O_lo
            if (this->current_output_ < this->olb_)
                this->current_mode_ = 1;
            else if ( (this->current_output_ >= this->olb_) && (this->current_output_ <= this->oub_) && (this->current_deadband_))
                this->current_mode_ = 0;
            break;
      }	


	  if (this->current_mode_ != this->previous_mode_) {
    
        if (this->current_mode_ == 1) {          // → CHARGE
        // On se place juste à la frontière basse
        // O_to_Oc(olb) = 0, mais le PID descend dès le cycle suivant
          this->previous_output_ = this->olb_;
          this->current_output_  = this->olb_;
        }
        else if (this->current_mode_ == 2) {     // → DISCHARGE
        // Idem côté décharge
          this->previous_output_ = this->oub_;
          this->current_output_  = this->oub_;
        }
        else {                                   // → IDLE
          this->previous_output_ = this->oneutral_;
          this->current_output_  = this->oneutral_;
       }

       this->previous_mode_  = this->current_mode_;
       this->last_time_      = now;
       this->previous_error_ = this->error_;
       return;
     }	
		
	 switch (this->previous_mode_) {
		 
        case 0:
			this->current_output_charging_    = 0.0f;	
	        this->current_output_discharging_ = 0.0f;
			this->current_onoff_              = false;
			break;

        case 1:
            this->current_output_charging_    = O_to_Oc(this->current_output_);   // O ∈ [0 – 0.5] → Oc ∈ [1 – 0]
            this->current_output_discharging_ = 0.0f;
			this->current_onoff_              = true;
            break;

        case 2:
 			this->current_output_charging_    = 0.0f;
			this->current_output_discharging_ = O_to_Od(this->current_output_);  // O ∈ [0.5 – 1] → Od ∈ [0 – 1]
            this->current_onoff_              = true;
            break;
      }	

	  this->current_output_charging_    = std::min(std::max( this->current_output_charging_ , this->current_output_min_charging_ ) , this->current_output_max_charging_);
	  this->current_output_discharging_ = std::min(std::max( this->current_output_discharging_ , this->current_output_min_discharging_ ) , this->current_output_max_discharging_);	
	
      if (!std::isnan(this->current_battery_voltage_)){
	    ESP_LOGI(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f" , this->current_battery_voltage_, this->current_starting_battery_voltage_);	
        if (this->current_battery_voltage_ < this->current_starting_battery_voltage_){
		  this->current_output_charging_    = 0.0f;
	      this->current_output_discharging_ = 0.0f;
		  this->previous_output_            = 0.5f;
		  this->current_output_             = 0.5f;
		  this->previous_mode_              = 0;
		  this->current_mode_               = 0;	
		  this->current_onoff_              = false;	
		  
		  if (this->onoff_switch_ != nullptr) {
           this->onoff_switch_->publish_state(false);
           this->onoff_switch_->turn_off();
           delay(ONOFF_DELAY);
		  }	
		  if (this->discharge_charge_switch_ != nullptr) {
            this->discharge_charge_switch_->publish_state(true);
            this->discharge_charge_switch_->turn_on();
            delay(CHARGE_DISCHARGE_DELAY); 
          }
        }
      }

	  if (!this->current_activation_ ){  // no regulation 
	    this->current_output_charging_    = 0.0f;
	    this->current_output_discharging_ = 0.0f;
		this->previous_output_            = this->oneutral_;
		this->current_output_             = this->oneutral_;
		this->previous_mode_              = 0;
		this->current_mode_               = 0;
		this->current_onoff_              = false;
		  
	    if( (this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)  ){
		  this->onoff_switch_->turn_off();
		  this->onoff_switch_->publish_state(false);	
		  delay(ONOFF_DELAY);
		  
		  if (this->discharge_charge_switch_ != nullptr) {	
		    this->discharge_charge_switch_->turn_on();
		    this->discharge_charge_switch_->publish_state(true);	
		    delay(CHARGE_DISCHARGE_DELAY);
		  }
		  ESP_LOGI(TAG, "activation is off -> Turn off onoff, turn on discharge_charge");	
        }
        this->previous_output_charging_    = this->current_output_charging_;
        this->previous_output_discharging_ = this->current_output_discharging_;
        this->pid_computed_callback_.call();
       return;   // ← indispensable 
      }	
	  else{
       if (!this->current_deadband_){ // Not in deadband
          if (this->discharge_charge_switch_ != nullptr) {
 	        if((this->current_output_charging_ > this->current_output_min_charging_) && (this->discharge_charge_switch_->state==false)){
			  this->discharge_charge_switch_->turn_on();
			  this->discharge_charge_switch_->publish_state(true);
		      delay(ONOFF_DELAY);
			  ESP_LOGI(TAG, "Turn on discharge_charge");	
            }
	        else if  ((this->current_output_discharging_ > this->current_output_min_discharging_) && (this->discharge_charge_switch_->state==true)){
			  this->discharge_charge_switch_->turn_off();
			  this->discharge_charge_switch_->publish_state(false);	
		      delay(CHARGE_DISCHARGE_DELAY);
			  ESP_LOGI(TAG, "Turn off discharge_charge");	
	        }
          }
	     }
	  }
		
	  if ((this->current_output_charging_ != this->previous_output_charging_) && (this->onoff_switch_ != nullptr) && (this->onoff_switch_->state==true) ){
        if (this->current_output_charging_ > 0.0f){ 
		  this->device_charging_output_->set_level(this->current_output_charging_);          // send command to PCM must be in [0.0 - 1.0] //
	      delay(SET_OUTPUT_DELAY);
		}
	  }
	  if ((this->current_output_discharging_ != this->previous_output_discharging_) && (this->onoff_switch_ != nullptr) && (this->onoff_switch_->state==true) ){  
	    if (this->current_output_discharging_ > 0.0f){ 
		  this->device_discharging_output_->set_level(this->current_output_discharging_);    // send command to PCM, must be in [0.0 - 1.0] //
          delay(SET_OUTPUT_DELAY);
		}
	  }
	  if (this->current_activation_ ){  
		 if (this->onoff_switch_ != nullptr) {
           should_be_on = (this->current_output_charging_  > 0.0f) || (this->current_output_discharging_ > 0.0f);
           if (should_be_on && !this->onoff_switch_->state) {
             this->onoff_switch_->turn_on();
             this->onoff_switch_->publish_state(true);
             delay(ONOFF_DELAY);
           } 
		   else if (!should_be_on && this->onoff_switch_->state) {
             this->onoff_switch_->turn_off();
             this->onoff_switch_->publish_state(false);
             delay(ONOFF_DELAY);
           }
         } 
	   }
	
      this->last_time_                   = now;
      this->previous_error_              = this->error_;
	  this->previous_output_             = this->current_output_;	
	  this->previous_output_charging_    = this->current_output_charging_;
	  this->previous_output_discharging_ = this->current_output_discharging_;

      this->pid_computed_callback_.call();		
	
	 }
	
   }
 }  // namespace dualpidpcm
}  // namespace esphome
