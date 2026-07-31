#include "dualpidpcm.h"
#include "esphome/core/log.h"

#define SET_OUTPUT_DELAY       0   // 50
#define ONOFF_DELAY            10  // 50
#define CHARGE_DISCHARGE_DELAY 0   // 50
#define DEADBAND_FACTOR        1.02
#define STARTUP_INHIBIT_MS     6000
#define DELAY_FEEDFORWARD      4000

namespace esphome {
namespace dualpidpcm {

static const char *const TAG = "dualpidpcm";

static const float coeffP = 0.00001f;
static const float coeffI = 0.00001f;
static const float coeffD = 0.00001f;

struct CalibrationPoint {
    float watts;
    float output_jump;
};


static const CalibrationPoint ff_table[] = {
    {0.0f,    0.000f},
    {281.0f,  0.0325f}, // 6.5%   -> 0.065 * 0.5 = 0.0325
    {712.6f,  0.083f},  // 16.6%  -> 0.166 * 0.5 = 0.083
    {1393.0f, 0.1695f}, // 33.9%  -> 0.339 * 0.5 = 0.1695
    {1856.5f, 0.229f},  // 45.8%  -> 0.458 * 0.5 = 0.229
    {2290.0f, 0.297f},  // 59.4%  -> 0.594 * 0.5 = 0.297
    {2500.0f, 0.3205f}, // 64.1%  -> 0.641 * 0.5 = 0.3205
    {3184.0f, 0.415f},  // 83.0%  -> 0.830 * 0.5 = 0.415
    {3450.0f, 0.475f},  // 95.0%  -> 0.950 * 0.5 = 0.475
    {3630.0f, 0.500f}   // Extrapolation 100% décharge
};

static const int ff_table_size = sizeof(ff_table) / sizeof(ff_table[0]);


float DUALPIDPCMComponent::calculate_ff_jump(float delta_w) {
    float abs_w = std::abs(delta_w);
    float abs_jump = 0.0f;

    if (abs_w <= ff_table[0].watts) {
        abs_jump = ff_table[0].output_jump;
    } else if (abs_w >= ff_table[ff_table_size - 1].watts) {
        abs_jump = ff_table[ff_table_size - 1].output_jump;
    } else {
        for (int i = 0; i < ff_table_size - 1; i++) {
            if (abs_w >= ff_table[i].watts && abs_w <= ff_table[i+1].watts) {
                float w1 = ff_table[i].watts;
                float j1 = ff_table[i].output_jump;
                float w2 = ff_table[i+1].watts;
                float j2 = ff_table[i+1].output_jump;
                abs_jump = j1 + (j2 - j1) * ((abs_w - w1) / (w2 - w1));
                break;
            }
        }
    }

    return (delta_w < 0) ? -abs_jump : abs_jump;
}



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
void DUALPIDPCMComponent::set_charging_level(float level) {
    float quantized = std::round(level * 1000.0f) / 1000.0f;
    if (quantized != this->previous_output_charging_) {
        if (quantized > 0.0f) {
            if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
                this->device_charging_output_->set_level(quantized);
                delay(SET_OUTPUT_DELAY);
                ESP_LOGD(TAG, "set_charging_level: %.4f", quantized);
            }
        }
    }
    this->current_output_charging_  = quantized;
    this->previous_output_charging_ = quantized;
}

void DUALPIDPCMComponent::set_discharging_level(float level) {
    float quantized = std::round(level * 1000.0f) / 1000.0f;
    if (quantized != this->previous_output_discharging_) {
        if (quantized > 0.0f) {
            if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
                this->device_discharging_output_->set_level(quantized);
                delay(SET_OUTPUT_DELAY);
                ESP_LOGD(TAG, "set_discharging_level: %.4f", quantized);
            }
        }
    }
    this->current_output_discharging_  = quantized;
    this->previous_output_discharging_ = quantized;
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
    this->pass_through_                = false;
    this->undervoltage_lockout_        = false;

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

    if (this->discharge_charge_switch_ != nullptr) {
      this->discharge_charge_switch_->publish_state(true);
      this->discharge_charge_switch_->turn_on();
      delay(CHARGE_DISCHARGE_DELAY);
    }

    ESP_LOGI(TAG, "setup: battery_voltage=%3.2f, pid_mode = %d",
             this->current_battery_voltage_, this->current_pid_mode_);
}


void DUALPIDPCMComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "dump config:");
    ESP_LOGVV(TAG, "setup import part: battery_voltage=%3.2f", this->current_battery_voltage_);
}


// ── pid_update ────────────────────────────────────────────────────────────────

void DUALPIDPCMComponent::pid_update() {
    uint32_t now = millis();
    float tmp, tmp_i, epsi;
    float alphaP, alphaI, alphaD, alpha;
    bool should_be_on, raw_deadband, output_is_active;
    bool in_startup, outputs_at_rest;
    float o_min_charge, o_max_charge, o_min_discharge, o_max_discharge, o_clamped;
    float delta_error, pending_jump;
    bool trigger_ff = false;
    static uint32_t last_ff_time = 0;
    float error_for_PID, error_for_D;
    float Pstart_charging, Pstart_discharging;

    ESP_LOGI(TAG, "Entered in pid_update()");
    ESP_LOGI(TAG, "Current pid mode %d", this->current_pid_mode_);

    if (this->current_manual_override_) {
        return;
    }

    // ── Garde dt ──────────────────────────────────────────────────────
    this->dt_ = float(now - this->last_time_) / 1000.0f;
    if (this->dt_ < 0.001f) {
        epsi         = this->current_input_ - this->current_setpoint_;
        this->error_ = epsi;
        if (this->current_reverse_) this->error_ = -this->error_;
        this->current_error_ = this->error_;

        this->last_time_                   = now;
        this->previous_output_charging_    = this->current_output_charging_;
        this->previous_output_discharging_ = this->current_output_discharging_;
        this->pid_computed_callback_.call();
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

    // ── Seuils de puissance ─────────────────────────────────────────────
    //
    // Deux seuils DISTINCTS par direction (hystérésis / trigger de Schmitt) :
    //
    //   Pstop_*  (= Pmin_charging / Pmin_discharging) : seuil d'ARRÊT.
    //       Motivé par la physique : Pmin_discharging inclut déjà
    //       current_self_consumption_ (autoconso à vide du convertisseur en
    //       décharge, ~30W typique) — décharger n'a de sens que si la conso
    //       maison dépasse ce que le convertisseur consomme lui-même.
    //
    //   Pstart_* (= Pstop_* ± current_delta_idle_*_)  : seuil de REDÉMARRAGE.
    //       Motivé par la stabilité : après un arrêt, il faut que la
    //       situation s'aggrave d'au moins current_delta_idle_*_ (W) avant
    //       de rallumer — évite le cyclage rapide autour du seuil d'arrêt.
    //
    // Les deux notions sont complémentaires, pas redondantes : Pstop_* fixe
    // OÙ s'arrêter (motif physique), current_delta_idle_*_ fixe DE COMBIEN il
    // faut s'écarter avant de repartir (motif anti-cyclage).
    this->Pmin_charging    = -this->current_battery_voltage_ * this->current_min_charging_;
    this->Pmin_discharging =  this->current_battery_voltage_ * this->current_min_discharging_
                             + this->current_self_consumption_;

    Pstart_charging    = this->Pmin_charging    - this->current_delta_idle_charging_;
    Pstart_discharging = this->Pmin_discharging + this->current_delta_idle_discharging_;

    // ── Deadband (utilise les seuils d'ARRÊT, inchangé) ────────────────
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
        this->pass_through_               = false;

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
        this->pid_computed_callback_.call();
        return;
    }

    // ── Deadband depuis mode ACTIF : arrêt réel ───────────────────────
    if (this->current_deadband_ && this->previous_mode_ != 0) {
        this->pass_through_ = false;

        if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
            this->onoff_switch_->turn_off();
            this->onoff_switch_->publish_state(false);
            delay(ONOFF_DELAY);
        }
        this->set_charging_level(0.0f);
        this->set_discharging_level(0.0f);
        this->current_onoff_ = false;

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
        this->pid_computed_callback_.call();
        return;
    }


    // ── Régulation PID (uniquement en mode ACTIF, jamais en IDLE) ──────
    // Depuis le passage à l'hystérésis Watts (Pstart_*), current_output_ ne
    // pilote plus aucune décision de transition pendant IDLE — l'entrée en
    // CHARGE/DISCHARGE dépend uniquement de epsi vs Pstart_charging/
    // Pstart_discharging (case 0 ci-dessous). Laisser le PID tourner en
    // continu pendant IDLE n'a donc plus aucune utilité : current_output_
    // dérivait librement (sans aucun clamp, celui-ci n'existant que dans
    // les branches CHARGE/DISCHARGE) jusqu'à saturer à 100%, alors que rien
    // n'était réellement envoyé (output_charging_/discharging_ restent à 0
    // en IDLE) — comportement trompeur en diagnostic et signe d'un calcul
    // PID inutile. On gèle donc current_output_ tant qu'on est en IDLE :
    // il reste à oneutral_ (position posée à l'entrée en IDLE) jusqu'à la
    // prochaine vraie transition de mode, qui le réinitialise de toute
    // façon explicitement à olb_/oub_.
    if (this->previous_mode_ != 0) {

    if(this->current_feedforward_){
      in_startup = (now - this->mode_start_time_) < STARTUP_INHIBIT_MS;  
      delta_error = this->error_ - this->previous_error_;
      if (std::abs(delta_error) > this->current_feedforward_threshold_) {
        pending_jump = calculate_ff_jump(delta_error);
        if ((now - last_ff_time) < DELAY_FEEDFORWARD) {
          ESP_LOGD(TAG, "Feed-Forward IGNORE (Attente réaction physique de l'onduleur) : delta=%.2f W", delta_error);
        }
        else {
          trigger_ff = true;
          last_ff_time = now;
          ESP_LOGD(TAG, "Feed-Forward DECLENCHE : Saut de %.2f W -> Ajustement sortie de %.4f", delta_error, pending_jump);
        }  
      }    
    }

    // ── Calcul PID ────────────────────────────────────────────────────
    error_for_PID = this->error_;
    error_for_D = this->error_;
    tmp_i = this->error_ * this->dt_;
    if (trigger_ff && !in_startup && std::abs(pending_jump) > 0.001f) {
      tmp_i = 0.0f;
      error_for_PID = 0.0f;
      error_for_D = this->previous_error_;
    }    
    if (!std::isnan(tmp_i)) this->integral_ += tmp_i;
    this->derivative_ = (error_for_D - this->previous_error_) / this->dt_;    

    tmp = 0.0f;
    if (!std::isnan(this->previous_output_) && !this->current_pid_mode_) {
        tmp = this->previous_output_;
    }

    if(this->current_feedforward_){
      if(trigger_ff && !in_startup && std::abs(pending_jump) > 0.001f){
        tmp += pending_jump;
        tmp = std::min(std::max(tmp, this->output_min_), this->output_max_);
        this->previous_error_ = this->error_;  
      }   
    }

    alphaP                = coeffP * this->current_kp_ * error_for_PID;    
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

        in_startup = (now - this->mode_start_time_) < STARTUP_INHIBIT_MS;

        if (in_startup) {
            this->current_output_  = this->oub_;
            this->integral_        = 0.0f;
            this->previous_output_ = this->oub_;
            this->previous_error_  = this->error_;
            ESP_LOGD(TAG, "DISCHARGE startup freeze: output held at oub=%.4f", this->oub_);
        }
    }

    }  // end if (previous_mode_ != 0) — régulation PID

    // ── Recalcul in_startup pour la machine d'état ────────────────────
    in_startup = (now - this->mode_start_time_) < STARTUP_INHIBIT_MS;

    // ── Machine d'état ────────────────────────────────────────────────
    // ENTRÉE (depuis IDLE) et BASCULE DIRECTE utilisent désormais toutes les
    // deux le seuil de REDÉMARRAGE (Pstart_*), directement sur epsi — et non
    // plus sur current_output_ vs olb_/oub_ (ancien mécanisme en espace O,
    // déconnecté des seuils physiques en W). olb_/oub_ restent utilisés
    // uniquement pour le clamp physique et le point d'atterrissage.
    //
    // SORTIE (arrêt réel) continue d'utiliser le seuil d'ARRÊT (Pmin_*, via
    // current_deadband_) — c'est la différence stop/start qui crée
    // l'hystérésis voulue.
    //
    // current_allow_charging_/current_allow_discharging_ verrouillent
    // toujours l'entrée/bascule vers le mode correspondant, priorité absolue
    // en sortie forcée si le flag passe à false en cours de fonctionnement.
    this->current_mode_ = this->previous_mode_;

    switch (this->previous_mode_) {
        case 0:  // IDLE
            if (epsi < Pstart_charging * DEADBAND_FACTOR && this->current_allow_charging_)
                this->current_mode_ = 1;
            else if (epsi > Pstart_discharging * DEADBAND_FACTOR && this->current_allow_discharging_)
                this->current_mode_ = 2;
            break;

        case 1:  // CHARGE
           // Verrou utilisateur : priorité absolue, arrêt réel immédiat
           if (!this->current_allow_charging_) {
             this->current_mode_ = 0;
             this->pass_through_ = false;
           }
           // Sortie directe vers DISCHARGE si output franchit oub_ (rare, code défensif)
           else if (this->current_output_ > this->oub_ && this->current_allow_discharging_) {
             this->current_mode_ = 2;
             this->pass_through_ = true;
           }
           // Arrêt réel : deadband confirmée (seuil d'ARRÊT) + sortie déjà au minimum
           else if (this->current_deadband_ && (this->current_output_charging_ <= this->current_output_min_charging_ + 0.01f) && !in_startup) {
             this->current_mode_ = 0;
             this->pass_through_ = false;
           }
           // Bascule (seuil de REDÉMARRAGE) : erreur franchement positive
           // + sortie déjà au minimum + décharge autorisée
           else if (!in_startup && (this->current_output_charging_ <= this->current_output_min_charging_ + 0.01f) && (epsi > Pstart_discharging * DEADBAND_FACTOR) && this->current_allow_discharging_) {
             this->current_mode_ = 0;   // → IDLE, qui basculera en DISCHARGE
             this->pass_through_ = true;
           }
           break;

        case 2:  // DISCHARGE
           if (!this->current_allow_discharging_) {
             this->current_mode_ = 0;
             this->pass_through_ = false;
           }
           else if (this->current_output_ < this->olb_ && this->current_allow_charging_) {
             this->current_mode_ = 1;
             this->pass_through_ = true;
           }
           else if (this->current_deadband_ && (this->current_output_discharging_ <= this->current_output_min_discharging_ + 0.01f) && !in_startup) {
             this->current_mode_ = 0;
             this->pass_through_ = false;
           }
           else if (!in_startup && (this->current_output_discharging_ <= this->current_output_min_discharging_ + 0.01f) && (epsi < Pstart_charging * DEADBAND_FACTOR) && this->current_allow_charging_) {
             this->current_mode_ = 0;   // → IDLE, qui basculera en CHARGE
             this->pass_through_ = true;
           }
           break;
     }

    // ── Transition de mode ────────────────────────────────────────────
    if (this->current_mode_ != this->previous_mode_) {

        if (this->current_mode_ == 1) {        // → CHARGE
            this->previous_output_ = this->olb_;
            this->current_output_  = this->olb_;
            if (!this->pass_through_) {
                this->mode_start_time_ = now;
            }
        }
        else if (this->current_mode_ == 2) {   // → DISCHARGE
            this->previous_output_ = this->oub_;
            this->current_output_  = this->oub_;
            if (!this->pass_through_) {
                this->mode_start_time_ = now;
            }
        }
        else {                                  // → IDLE
            this->previous_output_ = this->oneutral_;
            this->current_output_  = this->oneutral_;
            this->set_charging_level(0.0f);
            this->set_discharging_level(0.0f);
            this->current_onoff_ = this->pass_through_;

            if (!this->pass_through_) {
                if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
                    this->onoff_switch_->turn_off();
                    this->onoff_switch_->publish_state(false);
                    delay(ONOFF_DELAY);
                }
                if (this->discharge_charge_switch_ != nullptr && this->discharge_charge_switch_->state == false) {
                    this->discharge_charge_switch_->turn_on();
                    this->discharge_charge_switch_->publish_state(true);
                    delay(CHARGE_DISCHARGE_DELAY);
                }
            }
        }

        this->previous_mode_  = this->current_mode_;
        this->last_time_      = now;
        this->previous_error_ = this->error_;
        this->pid_computed_callback_.call();
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

    // ── Protection sous-tension batterie (hystérésis) ─────────────────
    if (!std::isnan(this->current_battery_voltage_)) {
        ESP_LOGI(TAG, "battery_voltage = %2.2f, stop = %2.2f, start = %2.2f, lockout = %d",
                 this->current_battery_voltage_,
                 this->current_stopping_battery_voltage_,
                 this->current_starting_battery_voltage_,
                 this->undervoltage_lockout_);

        if (this->current_battery_voltage_ < this->current_stopping_battery_voltage_) {
            this->undervoltage_lockout_ = true;
        } else if (this->current_battery_voltage_ >= this->current_starting_battery_voltage_) {
            this->undervoltage_lockout_ = false;
        }

        if (this->undervoltage_lockout_) {
            this->set_charging_level(0.0f);
            this->set_discharging_level(0.0f);
            this->current_output_             = this->oneutral_;
            this->previous_output_            = this->oneutral_;
            this->previous_mode_              = 0;
            this->current_mode_               = 0;
            this->current_onoff_              = false;
            this->current_deadband_           = false;
            this->pass_through_               = false;

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
            ESP_LOGI(TAG, "Turn on charge mode");
        }
        else if ((this->current_output_discharging_ > this->current_output_min_discharging_) && (this->discharge_charge_switch_->state == true)) {
            this->discharge_charge_switch_->turn_off();
            this->discharge_charge_switch_->publish_state(false);
            delay(CHARGE_DISCHARGE_DELAY);
            ESP_LOGI(TAG, "Turn on discharge mode");
        }
    }

    // ── Envoi des consignes via les helpers ───────────────────────────
    if(this->current_output_charging_  > 0.0f && this->current_allow_charging_){
      if(!this->discharge_charge_switch_->state){
         this->discharge_charge_switch_->publish_state(true);
         this->discharge_charge_switch_->turn_on();
         delay(CHARGE_DISCHARGE_DELAY); 
      }
      this->set_charging_level(this->current_output_charging_);
    }
    if(this->current_output_discharging_  > 0.0f && this->current_allow_discharging_){
      if(this->discharge_charge_switch_->state){
         this->discharge_charge_switch_->publish_state(false);
         this->discharge_charge_switch_->turn_off();
         delay(CHARGE_DISCHARGE_DELAY); 
      }  
      this->set_discharging_level(this->current_output_discharging_);
    }

    // ── Gestion onoff_switch ──────────────────────────────────────────
    if (this->onoff_switch_ != nullptr) {
        should_be_on = (this->current_output_charging_  > 0.0f)
                    || (this->current_output_discharging_ > 0.0f)
                    || (this->previous_mode_ == 0 && this->pass_through_);

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

    ESP_LOGI(TAG, "out=%.4f Oc=%.4f Od=%.4f mode=%d deadband=%d startup=%d pass_through=%d allow_c=%d allow_d=%d Pstart_c=%.1f Pstart_d=%.1f",
             this->current_output_,
             this->current_output_charging_,
             this->current_output_discharging_,
             this->previous_mode_,
             this->current_deadband_,
             (int)in_startup,
             (int)this->pass_through_,
             (int)this->current_allow_charging_,
             (int)this->current_allow_discharging_,
             Pstart_charging,
             Pstart_discharging);

    this->last_time_      = now;
    this->previous_error_ = this->error_;
    this->previous_output_ = this->current_output_;

    this->pid_computed_callback_.call();
}

}  // namespace dualpidpcm
}  // namespace esphome
