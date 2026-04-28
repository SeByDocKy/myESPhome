#include "dualpid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpid {

// Facteur zone morte : |epsi| < Pmin * DEADBAND_FACTOR → on ne fait rien
#define DEADBAND_FACTOR  1.05f



static const char *const TAG = "dualpid";

static const float coeffPcharging = 0.0001f;
static const float coeffIcharging = 0.0001f;
static const float coeffDcharging = 0.001f;

static const float coeffPdischarging = 0.001f;
static const float coeffIdischarging = 0.0001f;
static const float coeffDdischarging = 0.001f;



void DUALPIDComponent::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up DUALPIDComponent...");
  
  this->last_time_                   =  millis();
  this->integral_                    = 0.0f;
  this->previous_error_              = 0.0f;
  this->previous_output_             = this->current_epoint_;	
  this->previous_output_charging_    = 0.0f;
  this->previous_output_discharging_ = 0.0f;
  this->previous_activation_         = false;
  this->current_mode_                = 0;     // 0=IDLE, 1=CHARGE, 2=DISCHARGE
  this->previous_mode_               = 0;
	
  
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
    // S'assurer que r48 démarre côté charge (sécurité)
  if (this->r48_general_switch_ != nullptr && this->r48_general_switch_->state == false) {
    this->r48_general_switch_->turn_on();
    this->r48_general_switch_->publish_state(true);
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
  bool should_be_on, raw_deadband, output_is_active;
  float o_min_charge, o_max_charge, o_min_discharge, o_max_discharge, o_clamped;	
  
  ESP_LOGI(TAG, "Entered in pid_update()");
  ESP_LOGI(TAG, "Current pid mode %d" , this->current_pid_mode_);
	
  this->pid_computed_callback_.call();	

  #ifdef USE_SWITCH
    if (this->current_manual_override_) return;
#endif

    // ── Garde dt ──────────────────────────────────────────────────────
    this->dt_ = float(now - this->last_time_) / 1000.0f;
    if (this->dt_ < 0.001f) {
        this->last_time_                   = now;
        this->previous_output_charging_    = this->current_output_charging_;
        this->previous_output_discharging_ = this->current_output_discharging_;
        return;
    }

    // ── Calcul du point de pivot epoint_ selon tension batterie ───────
    if (this->current_battery_voltage_ < this->current_discharged_battery_voltage_) {
        this->current_epoint_ = this->current_charging_epoint_;
    } else if (this->current_battery_voltage_ < this->current_charged_battery_voltage_) {
        this->current_epoint_ = this->current_absorbing_epoint_;
    } else {
        this->current_epoint_ = this->current_floating_epoint_;
    }

    // Bornes d'hystérésis autour de epoint_ (équivalent olb_/oub_)
    float elb = this->current_epoint_ - this->o_hysteresis_; //EPOINT_HYST;   // frontière basse  → CHARGE
	elb = std::min(std::max(elb, this->current_output_min_), this->current_output_max_);
    float eub = this->current_epoint_ + this->o_hysteresis_; //EPOINT_HYST;   // frontière haute  → DISCHARGE
	eub = std::min(std::max(eub, this->current_output_min_), this->current_output_max_);


    // ── Calcul de l'erreur ────────────────────────────────────────────
    epsi          = this->current_input_ - this->current_setpoint_;
    this->error_  = epsi;
#ifdef USE_SWITCH
    if (this->current_reverse_) this->error_ = -this->error_;
#endif
    this->current_error_ = this->error_;

    // ── Reset propre au passage activation off → on ───────────────────
    if (this->current_activation_ && !this->previous_activation_) {
        this->previous_output_ = this->current_epoint_;   // neutre = epoint_
        this->current_output_  = this->current_epoint_;
        this->previous_error_  = this->error_;
        this->integral_        = 0.0f;
        this->previous_mode_   = 0;
        this->current_mode_    = 0;
        ESP_LOGI(TAG, "Activation ON: reset to epoint=%.4f", this->current_epoint_);
    }
    this->previous_activation_ = this->current_activation_;

    // ── Bloc désactivation ────────────────────────────────────────────
#ifdef USE_SWITCH
    if (!this->current_activation_) {
        this->output_charging_    = 0.0f;
        this->output_discharging_ = 0.0f;
        this->current_output_     = this->current_epoint_;
        this->previous_output_    = this->current_epoint_;
        this->previous_mode_      = 0;
        this->current_mode_       = 0;
        this->last_time_          = now;   // évite dt_ aberrant au redémarrage

        if (this->r48_general_switch_ != nullptr && this->r48_general_switch_->state == false) {
            this->r48_general_switch_->turn_on();
            this->r48_general_switch_->publish_state(true);
        }
        this->device_charging_output_->set_level(0.0f);
        this->device_discharging_output_->set_level(0.0f);

        this->previous_output_charging_    = 0.0f;
        this->previous_output_discharging_ = 0.0f;
        this->pid_computed_callback_.call();
        return;   // ← indispensable
    }
#endif

    // ── Protection sous-tension batterie ─────────────────────────────
    if (!std::isnan(this->current_battery_voltage_)) {
        ESP_LOGI(TAG, "battery_voltage=%.2f, starting=%.2f",
                 this->current_battery_voltage_, this->current_starting_battery_voltage_);
        if (this->current_battery_voltage_ < this->current_starting_battery_voltage_) {
            this->output_charging_    = 0.0f;
            this->output_discharging_ = 0.0f;
            this->current_output_     = this->current_epoint_;
            this->previous_output_    = this->current_epoint_;
            this->previous_mode_      = 0;
            this->current_mode_       = 0;
            this->device_charging_output_->set_level(0.0f);
            this->device_discharging_output_->set_level(0.0f);
            this->previous_output_charging_    = 0.0f;
            this->previous_output_discharging_ = 0.0f;
            this->last_time_ = now;
            this->pid_computed_callback_.call();
            return;
        }
    }

    // ── Calcul deadband asymétrique ───────────────────────────────────
    //
    // R48 (charge)  : courant minimum = 3.5A
    //   → Pmin_charging = Vbatt × Imin_charge  (ex: 51.2 × 3.5 ≈ 179 W)
    //   → en dessous de ce surplus solaire, le R48 ne peut pas absorber
    //   → deadband côté charge : epsi > -Pmin_charging × DEADBAND_FACTOR
    //     (epsi négatif = surplus solaire injecté réseau)
    //
    // HMS (décharge) : courant minimum ≈ 0A (12W à 1% ≈ 0.2A ≈ négligeable)
    //   → Pmin_discharging ≈ 0 W
    //   → le HMS peut démarrer dès que epsi > 0 (la moindre conso réseau)
    //   → pas de deadband côté décharge : seuil = 0 W
    //
    // Résultat : la zone morte est ASYMÉTRIQUE
    //   [-Pmin_ch × k  ...  0]  → IDLE  (surplus trop faible pour charger,
    //                                      pas de conso → rien à faire)
    //   epsi < -Pmin_ch × k    → surplus suffisant → CHARGE
    //   epsi > 0               → moindre conso → DISCHARGE

    float Pmin_ch  = this->current_battery_voltage_ * this->current_min_charging_;
    // Pmin_dis = 0 pour le HMS : on utilise un seuil symbolique > 0
    // pour éviter des oscillations sur le bruit de mesure
    float Pmin_dis = this->current_battery_voltage_ * this->current_min_discharging_;
    // Si current_min_discharging_ = 0 dans la config, Pmin_dis = 0
    // et la deadband côté décharge est quasi nulle — comportement voulu.

    // deadband = vrai si epsi est dans la zone où aucun appareil ne peut agir :
    //   - surplus trop faible pour que le R48 charge  (epsi > -Pmin_ch × k)
    //   - conso trop faible pour que le HMS décharge  (epsi < +Pmin_dis × k)
    raw_deadband = (epsi > -(Pmin_ch  * DEADBAND_FACTOR))   // pas assez de surplus
               && (epsi <  (Pmin_dis * DEADBAND_FACTOR));   // pas assez de conso

    // Inhibe la deadband si une sortie physique est déjà active
    // (évite la coupure prématurée pendant le démarrage ~3-4s du R48)
    output_is_active = (this->current_output_charging_   > this->current_output_min_charging_)
                    || (this->current_output_discharging_ > this->current_output_min_discharging_);

    this->current_deadband_ = raw_deadband && !output_is_active;

    ESP_LOGI(TAG, "deadband: epsi=%.1f Pmin_ch=%.1f Pmin_dis=%.1f raw=%d active=%d db=%d",
             epsi, Pmin_ch, Pmin_dis, raw_deadband, output_is_active, this->current_deadband_);

    // ── Deadband en mode IDLE : on reste off ──────────────────────────
    if (this->current_deadband_ && this->previous_mode_ == 0) {
        this->output_charging_             = 0.0f;
        this->output_discharging_          = 0.0f;
        this->last_time_                   = now;
        this->previous_error_              = this->error_;
        this->previous_output_charging_    = 0.0f;
        this->previous_output_discharging_ = 0.0f;
        this->pid_computed_callback_.call();
        return;
    }

    // ── Deadband depuis mode ACTIF : arrêt immédiat ───────────────────
    if (this->current_deadband_ && this->previous_mode_ != 0) {
        this->output_charging_    = 0.0f;
        this->output_discharging_ = 0.0f;

        // Repartir à la frontière du mode quitté pour redémarrer vite
        if (this->previous_mode_ == 2) {
            this->previous_output_ = eub;
            this->current_output_  = eub;
        } else {
            this->previous_output_ = elb;
            this->current_output_  = elb;
        }
        this->previous_mode_ = 0;
        this->current_mode_  = 0;

        if (this->r48_general_switch_ != nullptr && this->r48_general_switch_->state == false) {
            this->r48_general_switch_->turn_on();
            this->r48_general_switch_->publish_state(true);
        }
        this->device_charging_output_->set_level(0.0f);
        this->device_discharging_output_->set_level(0.0f);

        this->last_time_                   = now;
        this->previous_error_              = this->error_;
        this->previous_output_charging_    = 0.0f;
        this->previous_output_discharging_ = 0.0f;
        this->pid_computed_callback_.call();
        return;
    }

    // ── PID — coefficients selon le mode courant ──────────────────────
    if (this->previous_mode_ == 1) {   // CHARGE
        coeffP = coeffPcharging    * this->current_kp_charging_;
        coeffI = coeffIcharging    * this->current_ki_charging_;
        coeffD = coeffDcharging    * this->current_kd_charging_;
    } else {                           // DISCHARGE ou IDLE → gains décharge
        coeffP = coeffPdischarging * this->current_kp_discharging_;
        coeffI = coeffIdischarging * this->current_ki_discharging_;
        coeffD = coeffDdischarging * this->current_kd_discharging_;
    }

    tmp_i = this->error_ * this->dt_;
    if (!std::isnan(tmp_i)) this->integral_ += tmp_i;
    this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

    float tmp = 0.0f;
    if (!std::isnan(this->previous_output_) && !this->current_pid_mode_) {
        tmp = this->previous_output_;
    }

    alphaP = coeffP * this->error_;
    alphaI = coeffI * this->integral_;
    alphaD = coeffD * this->derivative_;
    alpha  = alphaP + alphaI + alphaD;

    this->current_output_ = std::min(std::max(tmp + alpha, this->current_output_min_), this->current_output_max_);

    // Anti-windup global
    if ((this->current_output_ <= this->current_output_min_)
     || (this->current_output_ >= this->current_output_max_)) {
        this->integral_ -= tmp_i;
    }

    ESP_LOGI(TAG, "PID: E=%.2f I=%.2f D=%.2f alpha=%.6f prev=%.4f out=%.4f",
             this->error_, this->integral_, this->derivative_,
             alpha, tmp, this->current_output_);

    // ── Machine d'état ────────────────────────────────────────────────
    this->current_mode_ = this->previous_mode_;

    switch (this->previous_mode_) {
        case 0:  // IDLE
            if (this->current_output_ < elb)
                this->current_mode_ = 1;   // → CHARGE
            else if (this->current_output_ > eub)
                this->current_mode_ = 2;   // → DISCHARGE
            break;

        case 1:  // CHARGE
            if (this->current_output_ > eub)
                this->current_mode_ = 2;
            else if ((this->current_output_ >= elb)
                  && (this->current_output_ <= eub)
                  && this->current_deadband_)
                this->current_mode_ = 0;
            break;

        case 2:  // DISCHARGE
            if (this->current_output_ < elb)
                this->current_mode_ = 1;
            else if ((this->current_output_ >= elb)
                  && (this->current_output_ <= eub)
                  && this->current_deadband_)
                this->current_mode_ = 0;
            break;
    }

    // ── Transition de mode ────────────────────────────────────────────
    if (this->current_mode_ != this->previous_mode_) {

        if (this->current_mode_ == 1) {        // → CHARGE
            // Commuter r48 en mode charge AVANT d'envoyer la consigne
            if (this->r48_general_switch_ != nullptr
             && this->r48_general_switch_->state == false) {
                this->r48_general_switch_->turn_on();
                this->r48_general_switch_->publish_state(true);
            }
            this->previous_output_ = elb;
            this->current_output_  = elb;

        } else if (this->current_mode_ == 2) { // → DISCHARGE
            // Commuter r48 en mode décharge
            if (this->r48_general_switch_ != nullptr
             && this->r48_general_switch_->state == true) {
                this->r48_general_switch_->turn_off();
                this->r48_general_switch_->publish_state(false);
            }
            this->previous_output_ = eub;
            this->current_output_  = eub;

        } else {                               // → IDLE
            this->previous_output_ = this->current_epoint_;
            this->current_output_  = this->current_epoint_;
        }

        // Couper les sorties pendant le cycle de transition
        this->output_charging_             = 0.0f;
        this->output_discharging_          = 0.0f;
        this->device_charging_output_->set_level(0.0f);
        this->device_discharging_output_->set_level(0.0f);

        this->previous_mode_               = this->current_mode_;
        this->last_time_                   = now;
        this->previous_error_              = this->error_;
        this->previous_output_charging_    = 0.0f;
        this->previous_output_discharging_ = 0.0f;
        this->pid_computed_callback_.call();
        return;
    }

    // ── Calcul des sorties physiques selon le mode ────────────────────
    switch (this->previous_mode_) {

        case 0:  // IDLE
            this->output_charging_    = 0.0f;
            this->output_discharging_ = 0.0f;
            break;

        case 1: {  // CHARGE  — output ∈ [0, elb] → Oc ∈ [Ocmax, 0]
            // Distance normalisée depuis elb vers 0
            // output = elb → Oc = 0,  output = 0 → Oc = max
            float span = (elb > 0.0f) ? elb : 1.0f;
            float oc   = (elb - this->current_output_) / span;
            this->output_charging_    = std::min(std::max(oc,
                                                  this->current_output_min_charging_),
                                                  this->current_output_max_charging_);
            this->output_discharging_ = 0.0f;
            // Anti-windup côté borne physique
            if ((this->output_charging_ <= this->current_output_min_charging_)
             || (this->output_charging_ >= this->current_output_max_charging_)) {
                this->integral_ -= tmp_i;
            }
            break;
        }

        case 2: {  // DISCHARGE  — output ∈ [eub, 1] → Od ∈ [0, Odmax]
            float span = (1.0f - eub > 0.0f) ? (1.0f - eub) : 1.0f;
            float od   = (this->current_output_ - eub) / span;
            this->output_charging_    = 0.0f;
            this->output_discharging_ = std::min(std::max(od,
                                                  this->current_output_min_discharging_),
                                                  this->current_output_max_discharging_);
            // Anti-windup côté borne physique
            if ((this->output_discharging_ <= this->current_output_min_discharging_)
             || (this->output_discharging_ >= this->current_output_max_discharging_)) {
                this->integral_ -= tmp_i;
            }
            break;
        }
    }

    // ── Commutation r48 selon la sortie effective ─────────────────────
    // (filet de sécurité — normalement géré dans les transitions)
    if (this->r48_general_switch_ != nullptr) {
        if ((this->output_charging_    > this->current_output_min_charging_)
         && !this->r48_general_switch_->state) {
            this->r48_general_switch_->turn_on();
            this->r48_general_switch_->publish_state(true);
            ESP_LOGI(TAG, "r48 turned ON (charge)");
        } else if ((this->output_discharging_ > this->current_output_min_discharging_)
                && this->r48_general_switch_->state) {
            this->r48_general_switch_->turn_off();
            this->r48_general_switch_->publish_state(false);
            ESP_LOGI(TAG, "r48 turned OFF (discharge)");
        }
    }

    // ── Envoi des consignes (uniquement si valeur a changé) ───────────
    if (this->output_charging_ != this->previous_output_charging_) {
        this->device_charging_output_->set_level(this->output_charging_);
    }
    if (this->output_discharging_ != this->previous_output_discharging_) {
#ifdef USE_BINARY_SENSOR
        if (this->producing_binary_sensor_ != nullptr
         && this->producing_binary_sensor_->state == true) {
#endif
            this->device_discharging_output_->set_level(this->output_discharging_);
#ifdef USE_BINARY_SENSOR
        }
#endif
    }

    ESP_LOGI(TAG, "out=%.4f Oc=%.4f Od=%.4f mode=%d deadband=%d",
             this->current_output_,
             this->output_charging_,
             this->output_discharging_,
             this->previous_mode_,
             this->current_deadband_);

    // ── Mise à jour des états précédents ──────────────────────────────
    this->last_time_                   = now;
    this->previous_error_              = this->error_;
    this->previous_output_             = this->current_output_;
    this->current_output_charging_     = this->output_charging_;
    this->current_output_discharging_  = this->output_discharging_;
    this->previous_output_charging_    = this->output_charging_;
    this->previous_output_discharging_ = this->output_discharging_;

    this->pid_computed_callback_.call();
}


	

//   if(this->current_battery_voltage_ < this->current_discharged_battery_voltage_){
// 	  this->current_epoint_ = this->current_charging_epoint_;
//   }
//   else if((this->current_battery_voltage_ >= this->current_discharged_battery_voltage_) && (this->current_battery_voltage_ < this->current_charged_battery_voltage_)){
// 	  this->current_epoint_ = this->current_absorbing_epoint_;
//   }
//   else{
//       this->current_epoint_ = this->current_floating_epoint_;
//   }   
//   if(this->current_epoint_ != 0.0f){	
//     cc = 1.0f/this->current_epoint_;
//   }
//   else{
// 	cc = 0.0f;
//   }
//   cd = 1.0f/(1.0f - this->current_epoint_);
	
//   e = (this->current_output_ < this->current_epoint_ ); // test if general regulation point is in charging domain or not
	
//   ESP_LOGI(TAG, "previous current_epoint: %2.5f, cc: %2.2f, cd: %2.2f, e: %d" , this->current_epoint_, cc, cd, e );	

// #ifdef USE_SWITCH  
//   if (!this->current_manual_override_){
// #endif
//     this->dt_    = float(now - this->last_time_)/1000.0f;
// 	tmp          = (this->current_input_ - this->current_setpoint_);  // initial epsilon error estimation
// 	this->error_ = tmp;  
	  
// #ifdef USE_SWITCH	  
// 	if (this->current_reverse_){
// 		this->error_ = -this->error_;
// 	}
// #endif	  
// 	this->current_error_ = this->error_;
	
//     tmp = (this->error_ * this->dt_);
//     if (!std::isnan(tmp)){
//       this->integral_ += tmp;
//     }
//     this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

//     tmp = 0.0f;
//     if( !std::isnan(this->previous_output_) && !this->current_pid_mode_){
//         tmp = this->previous_output_;
//     }
	
// 	ESP_LOGI(TAG, "previous output = %2.8f" , tmp );
// 	ESP_LOGI(TAG, "E = %3.2f, I = %3.2f, D = %3.2f, previous = %3.2f" , this->error_ , this->integral_ , this->derivative_ , tmp);
	
// 	if (e){
// 	  this->current_kp_ = this->current_kp_charging_;
// 	  this->current_ki_ = this->current_ki_charging_;
// 	  this->current_kd_ = this->current_kd_charging_;
      
// 	  coeffP = coeffPcharging*this->current_kp_;
// 	  coeffI = coeffIcharging*this->current_ki_;
// 	  coeffD = coeffDcharging*this->current_kd_;
		
// 	  alphaP = coeffP * this->error_;
// 	  alphaI = coeffI * this->integral_;
// 	  alphaD = coeffD * this->derivative_;
	
// 	}
// 	else{
// 	  this->current_kp_ = this->current_kp_discharging_;
// 	  this->current_ki_ = this->current_ki_discharging_;
// 	  this->current_kd_ = this->current_kd_discharging_;

// 	  coeffP = coeffPdischarging*this->current_kp_;
// 	  coeffI = coeffIdischarging*this->current_ki_;
// 	  coeffD = coeffDdischarging*this->current_kd_;	

// 	  alphaP = coeffP * this->error_;
// 	  alphaI = coeffI * this->integral_;
// 	  alphaD = coeffD * this->derivative_;
// 	}
	
// 	alpha  = alphaP + alphaI + alphaD;
	
//     this->output_ = std::min(std::max( tmp + alpha, this->current_output_min_ ) , this->current_output_max_);
	
//     ESP_LOGI(TAG, "Pcoeff = %3.8f" , alphaP );
// 	ESP_LOGI(TAG, "Icoeff = %3.8f" , alphaI );
// 	ESP_LOGI(TAG, "Dcoeff = %3.8f" , alphaD );
	
// 	ESP_LOGI(TAG, "output_min = %1.2f" , this->current_output_min_  );
// 	ESP_LOGI(TAG, "output_max = %1.2f" , this->current_output_max_  );
	
// 	ESP_LOGI(TAG, "PIDcoeff = %3.8f" , alpha );
	
// 	ESP_LOGI(TAG, "full pid update: setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_output_ = %3.2f , output_ = %3.2f , error_ = %3.2f, integral = %3.2f , derivative = %3.2f", this->current_target_ , coeffP*this->current_kp_ , coeffI*this->current_ki_ , coeffD*this->current_kd_ , this->current_output_min_ , this->current_output_max_ , this->previous_output_ , this->output_ , this->error_ , this->integral_ , this->derivative_);    
//     // this->last_time_       = now;
//     // this->previous_error_  = this->error_;
//     // this->previous_output_ = this->output_;
    
// 	ESP_LOGI(TAG, "activation %d", this->current_activation_);

// 	e   = (this->output_ < this->current_epoint_ );
// 	if(e){ // Charge <-> ACin (230V)->R48->DC 48V
//        tmp                       = (this->current_epoint_ - this->output_); // tmp is positive
// 	   this->output_charging_    = tmp; //cc*tmp; ?
// 	   this->output_discharging_ = 0.0f;	
// 	   this->output_charging_    = std::min(std::max( this->output_charging_ , this->current_output_min_charging_ ) , this->current_output_max_charging_);
// 	   this->previous_output_    = this->current_epoint_;
// 	}
// 	else{ // Discharge <-> Battery DC 48V->HMS->ACout (230V)
//        tmp                       = (this->output_ - this->current_epoint_ ); // tmp is positive
// 	   this->output_charging_    = 0.0f;
// 	   this->output_discharging_ = cd*tmp; // tmp;?
// 	   this->output_discharging_ = std::min(std::max( this->output_discharging_ , this->current_output_min_discharging_ ) , this->current_output_max_discharging_);	
// 	   this->previous_output_    = this->current_epoint_;
// 	}
// 	// tmp is a positive value

  
// #ifdef USE_SWITCH  
//     if (!this->current_activation_ ){
//       this->output_             = this->current_epoint_;
// 	  this->previous_output_    = this->current_epoint_;  	
// 	  this->output_charging_    = 0.0f;
// 	  this->output_discharging_ = 0.0f;	
//     }
// #endif  
	  
//     if (this->r48_general_switch_ != nullptr) {
//  	 if((this->output_charging_ >= this->current_output_min_charging_) & (this->r48_general_switch_->state==false)){
//        // this->r48_general_switch_->control(true);
// 	   this->r48_general_switch_->turn_on();	 
// 	   this->r48_general_switch_->publish_state(true);	
//      }
// 	 else if  ((this->output_discharging_ >= this->current_output_min_discharging_) & (this->r48_general_switch_->state==true)){
//        // this->r48_general_switch_->control(false);
// 	   this->r48_general_switch_->turn_off();	 
// 	   this->r48_general_switch_->publish_state(false);
// 	 }
//     }

//     if (!std::isnan(this->current_battery_voltage_)){
// 	  ESP_LOGI(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f" , this->current_battery_voltage_, this->current_starting_battery_voltage_);	
//       if (this->current_battery_voltage_ < this->current_starting_battery_voltage_){
//         this->output_             = this->current_epoint_;
// 		this->previous_output_    = this->current_epoint_;   
// 		this->output_charging_    = 0.0f;
// 	    this->output_discharging_ = 0.0f;  
//       }
//     }


// 	ESP_LOGI(TAG, "Final computed output=%1.6f, output_charging_=%1.6f, output_discharging_=%1.6f" , this->output_, this->output_charging_, this->output_discharging_);
// 	if (this->output_charging_ != this->previous_output_charging_){
//       this->device_charging_output_->set_level(this->output_charging_);          // send command to r48, must be in [0.0 - 1.0] //
// 	}
// 	if (this->output_discharging_ != this->previous_output_discharging_){  
// 	  this->device_discharging_output_->set_level(this->output_discharging_);    // send command to HMS, must be in [0.0 - 1.0] //
// 	}
// 	this->current_output_             = this->output_;  // must be in [0.0 - 1.0] //
// 	this->current_output_charging_    = this->output_charging_;
	  
// #ifdef USE_BINARY_SENSOR
// 	if (this->producing_binary_sensor_ != nullptr & this->producing_binary_sensor_->state==true){ // control HMS only when it's starting to produce if not... HMS is blocked
// #endif		
// 	this->current_output_discharging_ = this->output_discharging_;  
// #ifdef USE_BINARY_SENSOR
// 	}
// #endif
//     this->pid_computed_callback_.call();

//     this->last_time_                   = now;
//     this->previous_error_              = this->error_;
//     this->previous_output_             = this->output_;
// 	this->previous_output_charging_    = this->output_charging_;
// 	this->previous_output_discharging_ = this->output_discharging_;
	
// #ifdef USE_SWITCH	
//   } 
// #endif  
  
//  } // end pid_update






 }  // namespace dualpid
}  // namespace esphome
