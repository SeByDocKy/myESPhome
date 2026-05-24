#include "dualpidpcm.h"
#include "esphome/core/log.h"

#define SET_OUTPUT_DELAY       0   // 50
#define ONOFF_DELAY            0   // 50
#define CHARGE_DISCHARGE_DELAY 0   // 50
#define DEADBAND_FACTOR        1.02
#define STARTUP_INHIBIT_MS     5000

namespace esphome {
namespace dualpidpcm {

static const char *const TAG = "dualpidpcm";

static const float coeffP = 0.00001f;
static const float coeffI = 0.001f;
static const float coeffD = 0.001f;


// ── Helpers O_to_Oc / O_to_Od ────────────────────────────────────────────────

float DUALPIDPCMComponent::O_to_Oc(float O) {
    if (O > this->oneutral_) return 0.0f;
    return 1.0f - (O / this->oneutral_);
}

float DUALPIDPCMComponent::O_to_Od(float O) {
    if (O < this->oneutral_) return 0.0f;
    return (O - this->oneutral_) / (1.0f - this->oneutral_);
}


// ── Helpers d'envoi de consignes ─────────────────────────────────────────────
// N'appellent set_level que si la valeur a changé depuis le dernier envoi,
// comparé à previous_output_* (et non current_output_* qui peut être modifié
// manuellement avant l'appel). Cela évite de saturer le canal radio HMS/OpenDTU.

void DUALPIDPCMComponent::set_charging_level(float level) {
    float quantized = std::round(level * 10.0f) / 10.0f;
    if (quantized != this->previous_output_charging_) {
        if (quantized > 0.0f) {
            // N'envoyer une consigne active que si le switch est allumé
            if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
                this->device_charging_output_->set_level(quantized);
                delay(SET_OUTPUT_DELAY);
                ESP_LOGD(TAG, "set_charging_level: %.4f", quantized);
            }
        }
        // Pas de set_level pour level == 0 : c'est le onoff_switch qui coupe
    }
    this->current_output_charging_  = level;
    this->previous_output_charging_ = level;
}

void DUALPIDPCMComponent::set_discharging_level(float level) {
    float quantized = std::round(level * 10.0f) / 10.0f;
    if (quantized != this->previous_output_discharging_) {
        if (quantized > 0.0f) {
            if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
                this->device_discharging_output_->set_level(quantized);
                delay(SET_OUTPUT_DELAY);
                ESP_LOGD(TAG, "set_discharging_level: %.4f", quantized);
            }
        }
    }
    this->current_output_discharging_  = level;
    this->previous_output_discharging_ = level;
}


// ── Setup ─────────────────────────────────────────────────────────────────────

void DUALPIDPCMComponent::setup() {
    ESP_LOGCONFIG(TAG, "Setting up DUALPIDPCMComponent...");

    this->last_time_                   = millis();
    this->integral_                    = 0.0f;
    this->previous_error_              = 0.0f;
    this->previous_output_charging_    = 0.0f;
    this->previous_output_discharging_ = 0.0f;
    this->previous_activation_         = false;
    this->previous_mode_               = 0;
    this->current_mode_                = 0;
    this->mode_start_time_             = millis() - STARTUP_INHIBIT_MS;  // in_startup=false au boot

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

    if (this->onoff_switch_ != nullptr) {
        if (this->onoff_switch_->state == true) {
            this->onoff_switch_->turn_off();
            this->onoff_switch_->publish_state(false);
        }
    }

    this->olb_ = this->oneutral_ - this->lb_;
    this->oub_ = this->oneutral_ + this->ub_;

    this->pid_computed_callback_.call();

    ESP_LOGI(TAG, "setup: battery_voltage=%3.2f, pid_mode = %d",
             this->current_battery_voltage_, this->current_pid_mode_);
}


void DUALPIDPCMComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "dump config:");
    ESP_LOGVV(TAG, "setup import part: battery_voltage=%3.2f", this->current_battery_voltage_);
    this->pid_computed_callback_.call();
}


// ── pid_update ────────────────────────────────────────────────────────────────

void DUALPIDPCMComponent::pid_update() {
    uint32_t now = millis();
    float tmp, tmp_i, epsi;
    float alphaP, alphaI, alphaD, alpha;
    bool should_be_on, raw_deadband, output_is_active;
    bool in_startup, outputs_at_rest;
    float o_min_charge, o_max_charge, o_min_discharge, o_max_discharge, o_clamped;

    ESP_LOGI(TAG, "Entered in pid_update()");
    ESP_LOGI(TAG, "Current pid mode %d", this->current_pid_mode_);

    this->pid_computed_callback_.call();

    if (!this->current_manual_override_) {

    // ── Garde dt ──────────────────────────────────────────────────────
    this->dt_ = float(now - this->last_time_) / 1000.0f;
    if (this->dt_ < 0.001f) {
        this->last_time_                   = now;
        this->previous_output_charging_    = this->current_output_charging_;
        this->previous_output_discharging_ = this->current_output_discharging_;
        return;
    }

    // ── Calcul de l'erreur ────────────────────────────────────────────
    epsi         = this->current_input_ - this->current_setpoint_;
    this->error_ = epsi;
    if (this->current_reverse_) this->error_ = -this->error_;
    this->current_error_ = this->error_;

    // ── Reset propre au passage activation off → on ───────────────────
    if (this->current_activation_ && !this->previous_activation_) {
        this->previous_error_ = this->error_;
        this->integral_       = 0.0f;
        if (this->error_ > 0.0f) {
            this->previous_output_ = this->oub_;
            this->current_output_  = this->oub_;
        }
        else {
            this->previous_output_ = this->olb_;
            this->current_output_  = this->olb_;
        }
        this->previous_mode_ = 0;
        this->current_mode_  = 0;
    }

    this->previous_activation_ = this->current_activation_;

    // ── Seuils de puissance minimale ──────────────────────────────────
    this->Pmin_charging    = -this->current_battery_voltage_ * this->current_min_charging_;
    this->Pmin_discharging =  this->current_battery_voltage_ * this->current_min_discharging_;

    // ── Deadband ──────────────────────────────────────────────────────
    raw_deadband     = (epsi > this->Pmin_charging  * DEADBAND_FACTOR) && (epsi < this->Pmin_discharging * DEADBAND_FACTOR);
    output_is_active = (this->current_output_charging_  > this->current_output_min_charging_) || (this->current_output_discharging_ > this->current_output_min_discharging_);

    
    if (this->previous_mode_ == 1) {
      outputs_at_rest = (this->current_output_charging_  <= this->current_output_min_charging_); 
    }
    else if (this->previous_mode_ == 2) {
      outputs_at_rest = (this->current_output_discharging_ <= this->current_output_min_discharging_);
    }
    else {
      outputs_at_rest = true;  // IDLE : toujours au repos
    }    
        
    this->current_deadband_ = raw_deadband && !output_is_active && outputs_at_rest;

    // Forcer deadband=false si activation est off
    if (!this->current_activation_) {
        this->current_deadband_ = false;
    }

    this->current_mode_ = this->previous_mode_;

    // ── Désactivation ─────────────────────────────────────────────────
    if (!this->current_activation_) {
        this->set_charging_level(0.0f);
        this->set_discharging_level(0.0f);

        this->current_output_             = this->oneutral_;
        this->previous_output_            = this->oneutral_;
        this->previous_mode_              = 0;
        this->current_mode_               = 0;
        this->current_onoff_              = false;
        this->current_deadband_           = false;

        if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
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

        this->last_time_                   = now;
        this->previous_error_              = this->error_;
        this->pid_computed_callback_.call();
        return;
    }

    // ── Deadband en mode IDLE : on reste off ──────────────────────────
    if (this->current_deadband_ && this->previous_mode_ == 0) {
        if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
            this->onoff_switch_->turn_off();
            this->onoff_switch_->publish_state(false);
        }
        this->set_charging_level(0.0f);
        this->set_discharging_level(0.0f);
        this->last_time_                   = now;
        this->previous_error_              = this->error_;
        return;
    }

    // ── Deadband depuis mode ACTIF : arrêt immédiat ───────────────────
    if (this->current_deadband_ && this->previous_mode_ != 0) {
        if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
            this->onoff_switch_->turn_off();
            this->onoff_switch_->publish_state(false);
            delay(ONOFF_DELAY);
        }
        this->set_charging_level(0.0f);
        this->set_discharging_level(0.0f);
        this->current_onoff_ = false;

        // Repartir à la frontière du mode quitté
        if (this->previous_mode_ == 2) {
            this->previous_output_ = this->oub_;
            this->current_output_  = this->oub_;
        }
        else {
            this->previous_output_ = this->olb_;
            this->current_output_  = this->olb_;
        }
        this->previous_mode_  = 0;
        this->current_mode_   = 0;
        this->last_time_      = now;
        this->previous_error_ = this->error_;
        return;
    }

    // ── Calcul PID ────────────────────────────────────────────────────
    tmp_i = this->error_ * this->dt_;
    if (!std::isnan(tmp_i)) this->integral_ += tmp_i;
    this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

    tmp = 0.0f;
    if (!std::isnan(this->previous_output_) && !this->current_pid_mode_) {
        tmp = this->previous_output_;
    }

    alphaP                = coeffP * this->current_kp_ * this->error_;
    alphaI                = coeffI * this->current_ki_ * this->integral_;
    alphaD                = coeffD * this->current_kd_ * this->derivative_;
    alpha                 = alphaP + alphaI + alphaD;
    this->current_output_ = std::min(std::max(tmp + alpha, this->output_min_), this->output_max_);

    // ── Clamping O selon le mode courant ──────────────────────────────
    if (this->previous_mode_ == 1) {        // CHARGE
        o_min_charge = (1.0f - this->current_output_max_charging_) * this->oneutral_;
        o_max_charge = (1.0f - this->current_output_min_charging_) * this->oneutral_;
        o_clamped    = std::min(std::max(this->current_output_, o_min_charge), o_max_charge);
        if (o_clamped != this->current_output_) {
            if (tmp_i < 0.0f) this->integral_ -= tmp_i;
        }
        this->current_output_ = o_clamped;

        // ── Freeze pendant le démarrage du PCM ────────────────────────
        // Bloque output à olb_ (= Oc=0) le temps que le PCM soit opérationnel.
        // Remet previous_error_ à jour pour éviter un pic dérivé au déblocage.
        in_startup = (now - this->mode_start_time_) < STARTUP_INHIBIT_MS;
        if (in_startup) {
            this->current_output_  = this->olb_;
            this->integral_        = 0.0f;
            this->previous_output_ = this->olb_;
            this->previous_error_  = this->error_;
            ESP_LOGD(TAG, "CHARGE startup freeze: output held at olb=%.4f", this->olb_);
        }
    }
    else if (this->previous_mode_ == 2) {   // DISCHARGE
        o_min_discharge = this->current_output_min_discharging_ * this->oneutral_ + this->oneutral_;
        o_max_discharge = this->current_output_max_discharging_ * this->oneutral_ + this->oneutral_;
        o_clamped       = std::min(std::max(this->current_output_, o_min_discharge), o_max_discharge);
        if (o_clamped != this->current_output_) {
            if (tmp_i > 0.0f) this->integral_ -= tmp_i;
        }
        this->current_output_ = o_clamped;

        // ── Freeze pendant le démarrage du PCM (décharge) ─────────────
        in_startup = (now - this->mode_start_time_) < STARTUP_INHIBIT_MS;
        
        if (in_startup) {
            this->current_output_  = this->oub_;
            this->integral_        = 0.0f;
            this->previous_output_ = this->oub_;
            this->previous_error_  = this->error_;
            ESP_LOGD(TAG, "DISCHARGE startup freeze: output held at oub=%.4f", this->oub_);
        }
    }

    // ── Recalcul in_startup pour la machine d'état ────────────────────
    in_startup = (now - this->mode_start_time_) < STARTUP_INHIBIT_MS;

    // ── Machine d'état ────────────────────────────────────────────────
    this->current_mode_ = this->previous_mode_;

    switch (this->previous_mode_) {
        case 0:  // IDLE
            if (this->current_output_ < this->olb_)
                this->current_mode_ = 1;
            else if (this->current_output_ > this->oub_)
                this->current_mode_ = 2;
            break;

        case 1:  // CHARGE
          // Sortie directe vers DISCHARGE si output franchit oub_ (rare mais possible)
           if (this->current_output_ > this->oub_) {
             this->current_mode_ = 2;
           }
           // Retour IDLE : seulement si output_charging est au minimum ET deadband
           // (le PID a déjà ramené la charge à zéro avant de considérer l'arrêt)
           else if (this->current_deadband_ && (this->current_output_charging_ <= this->current_output_min_charging_ + 0.01f) && !in_startup) {
               this->current_mode_ = 0;
            }
           break;

        case 2:  // DISCHARGE
           // Sortie directe vers CHARGE si output franchit olb_
          if (this->current_output_ < this->olb_) {
            this->current_mode_ = 1;
          }
          // Retour IDLE : seulement si output_discharging est au minimum ET deadband
          else if (this->current_deadband_ && (this->current_output_discharging_ <= this->current_output_min_discharging_ + 0.01f) && !in_startup) {
             this->current_mode_ = 0;
          }
          break;
     }

    // ── Transition de mode ────────────────────────────────────────────
    if (this->current_mode_ != this->previous_mode_) {

        if (this->current_mode_ == 1) {          // → CHARGE
            this->previous_output_ = this->olb_;
            this->current_output_  = this->olb_;
            this->mode_start_time_ = now;          // ← arme le freeze STARTUP
        }
        else if (this->current_mode_ == 2) {     // → DISCHARGE
            this->previous_output_ = this->oub_;
            this->current_output_  = this->oub_;
            this->mode_start_time_ = now;          // ← arme le freeze STARTUP
        }
        else {                                    // → IDLE
            this->previous_output_ = this->oneutral_;
            this->current_output_  = this->oneutral_;
              // Couper les sorties physiques immédiatement
            this->set_charging_level(0.0f);
            this->set_discharging_level(0.0f);
            this->current_onoff_ = false;
            if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
              this->onoff_switch_->turn_off();
              this->onoff_switch_->publish_state(false);
              delay(ONOFF_DELAY);
            }
              // Remettre discharge_charge_switch en position charge (sécurité)
           if (this->discharge_charge_switch_ != nullptr && this->discharge_charge_switch_->state == false) {
             this->discharge_charge_switch_->turn_on();
             this->discharge_charge_switch_->publish_state(true);
             delay(CHARGE_DISCHARGE_DELAY);
           }
        }

        this->previous_mode_  = this->current_mode_;
        this->last_time_      = now;
        this->previous_error_ = this->error_;
        return;
    }

    // ── Calcul des sorties physiques ──────────────────────────────────
    switch (this->previous_mode_) {
        case 0:
            this->current_output_charging_    = 0.0f;
            this->current_output_discharging_ = 0.0f;
            this->current_onoff_              = false;
            break;

        case 1:
            this->current_output_charging_    = O_to_Oc(this->current_output_);
            this->current_output_discharging_ = 0.0f;
            this->current_onoff_              = true;
            break;

        case 2:
            this->current_output_charging_    = 0.0f;
            this->current_output_discharging_ = O_to_Od(this->current_output_);
            this->current_onoff_              = true;
            break;
    }

    this->current_output_charging_    = std::min(std::max(this->current_output_charging_, this->current_output_min_charging_), this->current_output_max_charging_);
    this->current_output_discharging_ = std::min(std::max(this->current_output_discharging_, this->current_output_min_discharging_), this->current_output_max_discharging_);

    // ── Protection sous-tension batterie ─────────────────────────────
    if (!std::isnan(this->current_battery_voltage_)) {
        ESP_LOGI(TAG, "battery_voltage = %2.2f, starting battery voltage = %2.2f", this->current_battery_voltage_, this->current_starting_battery_voltage_);
        if (this->current_battery_voltage_ < this->current_starting_battery_voltage_) {
            this->set_charging_level(0.0f);
            this->set_discharging_level(0.0f);
            this->current_output_             = this->oneutral_;
            this->previous_output_            = this->oneutral_;
            this->previous_mode_              = 0;
            this->current_mode_               = 0;
            this->current_onoff_              = false;
            this->current_deadband_           = false;

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
            this->last_time_      = now;
            this->previous_error_ = this->error_;
            this->pid_computed_callback_.call();
            return;
        }
    }

    // ── Gestion discharge_charge_switch ──────────────────────────────
    if (!this->current_deadband_ && this->discharge_charge_switch_ != nullptr) {
        if ((this->current_output_charging_ > this->current_output_min_charging_) && (this->discharge_charge_switch_->state == false)) {
            this->discharge_charge_switch_->turn_on();
            this->discharge_charge_switch_->publish_state(true);
            delay(ONOFF_DELAY);
            ESP_LOGI(TAG, "Turn on discharge_charge");
        }
        else if ((this->current_output_discharging_ > this->current_output_min_discharging_) && (this->discharge_charge_switch_->state == true)) {
            this->discharge_charge_switch_->turn_off();
            this->discharge_charge_switch_->publish_state(false);
            delay(CHARGE_DISCHARGE_DELAY);
            ESP_LOGI(TAG, "Turn off discharge_charge");
        }
    }

    // ── Envoi des consignes via les helpers ───────────────────────────
    // set_charging_level / set_discharging_level ne transmettent que si
    // la valeur a changé → évite de saturer le canal radio HMS/OpenDTU
    this->set_charging_level(this->current_output_charging_);
    this->set_discharging_level(this->current_output_discharging_);

    // ── Gestion onoff_switch ──────────────────────────────────────────
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

    ESP_LOGI(TAG, "out=%.4f Oc=%.4f Od=%.4f mode=%d deadband=%d startup=%d",
             this->current_output_,
             this->current_output_charging_,
             this->current_output_discharging_,
             this->previous_mode_,
             this->current_deadband_,
             (int)in_startup);

    // ── Mise à jour des états précédents ──────────────────────────────
    // current_output_charging_ et previous_output_charging_ sont gérés
    // par les helpers — ne pas les écraser ici
    this->last_time_      = now;
    this->previous_error_ = this->error_;
    this->previous_output_ = this->current_output_;

    this->pid_computed_callback_.call();

    }  // end if !manual_override
}

}  // namespace dualpidpcm
}  // namespace esphome
