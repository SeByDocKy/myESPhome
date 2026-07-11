#include "dualpidpcm.h"
#include "esphome/core/log.h"

#define SET_OUTPUT_DELAY       0   // 50
#define ONOFF_DELAY            10  // 50
#define CHARGE_DISCHARGE_DELAY 0   // 50
#define DEADBAND_FACTOR        1.02
#define STARTUP_INHIBIT_MS     6000

// ── Anti-cyclage adaptatif ──────────────────────────────────────────────────
// Si TRANSITION_HISTORY_SIZE transitions de mode se produisent en moins de
// CYCLING_WINDOW_MS, on considère qu'il y a du cyclage (nuages intermittents,
// charges courtes...) et on élargit l'hystérésis effective de ADAPTIVE_MARGIN_STEP
// (jusqu'à ADAPTIVE_MARGIN_MAX). La marge redescend automatiquement si aucune
// transition ne survient pendant ADAPTIVE_MARGIN_DECAY_MS.
// Une transition isolée n'est JAMAIS retardée : seule une salve rapprochée
// déclenche l'élargissement.
#define CYCLING_WINDOW_MS         120000  // 2 min
#define ADAPTIVE_MARGIN_STEP      0.02f   // +2% d'hystérésis à chaque détection
#define ADAPTIVE_MARGIN_MAX       0.08f   // plafond +8%
#define ADAPTIVE_MARGIN_DECAY_MS  180000  // 3 min de calme -> on relâche

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

// static const CalibrationPoint ff_table[] = {
//     {0.0f,    0.000f},
//     {900.0f,  0.125f},  // À vérifier : 900W correspondent-ils bien à 0.125 ?
//     {1800.0f, 0.250f},  // À vérifier : 1800W correspondent-ils bien à 0.250 ?
//     {2700.0f, 0.375f},  // À vérifier : 2700W correspondent-ils bien à 0.375 ?
//     {3600.0f, 0.500f}   // Puissance max
// };

static const int ff_table_size = sizeof(ff_table) / sizeof(ff_table[0]);


float DUALPIDPCMComponent::calculate_ff_jump(float delta_w) {
    float abs_w = std::abs(delta_w);
    float abs_jump = 0.0f;

    // Si on dépasse les bornes
    if (abs_w <= ff_table[0].watts) {
        abs_jump = ff_table[0].output_jump;
    } else if (abs_w >= ff_table[ff_table_size - 1].watts) {
        abs_jump = ff_table[ff_table_size - 1].output_jump;
    } else {
        // Recherche des deux points entre lesquels on se trouve (Interpolation linéaire)
        for (int i = 0; i < ff_table_size - 1; i++) {
            if (abs_w >= ff_table[i].watts && abs_w <= ff_table[i+1].watts) {
                float w1 = ff_table[i].watts;
                float j1 = ff_table[i].output_jump;
                float w2 = ff_table[i+1].watts;
                float j2 = ff_table[i+1].output_jump;
                
                // Produit en croix pour trouver la valeur exacte entre les deux points
                abs_jump = j1 + (j2 - j1) * ((abs_w - w1) / (w2 - w1));
                break;
            }
        }
    }

    // On redonne le bon signe (positif ou négatif)
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

// ── Anti-cyclage adaptatif ────────────────────────────────────────────────────

void DUALPIDPCMComponent::record_transition_(uint32_t now) {
    this->transition_history_[this->transition_idx_ % TRANSITION_HISTORY_SIZE] = now;
    this->transition_idx_++;

    if (this->transition_idx_ >= TRANSITION_HISTORY_SIZE) {
        uint32_t oldest = this->transition_history_[this->transition_idx_ % TRANSITION_HISTORY_SIZE];
        if ((now - oldest) < CYCLING_WINDOW_MS) {
            float before = this->adaptive_margin_;
            this->adaptive_margin_ = std::min(this->adaptive_margin_ + ADAPTIVE_MARGIN_STEP,
                                              (float) ADAPTIVE_MARGIN_MAX);
            if (this->adaptive_margin_ != before) {
                ESP_LOGW(TAG, "Cycling detected (%d transitions in %.0fs) -> adaptive_margin=%.3f",
                         TRANSITION_HISTORY_SIZE, (now - oldest) / 1000.0f, this->adaptive_margin_);
            }
        }
    }
}

void DUALPIDPCMComponent::decay_adaptive_margin_(uint32_t now) {
    if (this->adaptive_margin_ <= 0.0f) return;

    uint32_t last_transition = this->transition_history_[(this->transition_idx_ + TRANSITION_HISTORY_SIZE - 1)
                                                           % TRANSITION_HISTORY_SIZE];
    if ((now - last_transition) > ADAPTIVE_MARGIN_DECAY_MS) {
        this->adaptive_margin_ = std::max(this->adaptive_margin_ - ADAPTIVE_MARGIN_STEP, 0.0f);
        ESP_LOGI(TAG, "Adaptive margin decayed to %.3f", this->adaptive_margin_);
        // Réarme le timer de decay pour la prochaine étape progressive
        this->transition_history_[(this->transition_idx_ + TRANSITION_HISTORY_SIZE - 1)
                                   % TRANSITION_HISTORY_SIZE] = now;
    }
}


// ── Helpers d'envoi de consignes ─────────────────────────────────────────────
// N'appellent set_level que si la valeur a changé depuis le dernier envoi,
// comparé à previous_output_* (et non current_output_* qui peut être modifié
// manuellement avant l'appel). Cela évite de saturer le canal radio HMS/OpenDTU.

void DUALPIDPCMComponent::set_charging_level(float level) {
    float quantized = std::round(level * 1000.0f) / 1000.0f;
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
    this->adaptive_margin_             = 0.0f;
    this->transition_idx_              = 0;
    this->pass_through_                = false;
    this->ff_locked_                   = false;

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

    // Initialization of volatile registers (36,37,38)

    if (this->discharge_charge_switch_ != nullptr) {
      this->discharge_charge_switch_->publish_state(true);
      this->discharge_charge_switch_->turn_on();
      delay(CHARGE_DISCHARGE_DELAY);
    }

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
    float olb_eff, oub_eff;
    float delta_error, pending_jump;
    bool trigger_ff = false;
    static uint32_t last_ff_time = 0;
    float error_for_PID, error_for_D;

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

    // ── Anti-cyclage : relâche progressive de la marge adaptative ────
    this->decay_adaptive_margin_(now);

    // Bornes effectives utilisées pour la détection de transition depuis IDLE.
    // olb_/oub_ bruts restent utilisés pour O_to_Oc/O_to_Od (conversion physique).
    olb_eff = this->olb_ - this->adaptive_margin_;
    oub_eff = this->oub_ + this->adaptive_margin_;

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
        return;
    }

    // ── Deadband depuis mode ACTIF : arrêt réel ───────────────────────
    // C'est toujours un arrêt réel (jamais une bascule) : plus de besoin
    // ni de charge ni de décharge -> on coupe onoff_switch_.
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
        this->record_transition_(now);   // ← anti-cyclage
        return;
    }

        
    if(this->current_feedforward_){
      in_startup = (now - this->mode_start_time_) < STARTUP_INHIBIT_MS;  
      delta_error = this->error_ - this->previous_error_;
      // pending_jump = 0.0f;
      if (std::abs(delta_error) > this->current_feedforward_threshold_) {
        pending_jump = calculate_ff_jump(delta_error);
        if ((now - last_ff_time) < 8000) {
          ESP_LOGD(TAG, "Feed-Forward IGNORE (Attente réaction physique de l'onduleur) : delta=%.2f W", delta_error);
        } 
        else {
          trigger_ff = true;
          last_ff_time = now; // On démarre le chrono de verrouillage
          ESP_LOGD(TAG, "Feed-Forward DECLENCHE : Saut de %.2f W -> Ajustement sortie de %.4f", delta_error, pending_jump);
        }  
        // if (this->ff_locked_) {
        //   ESP_LOGD(TAG, "Feed-Forward candidat IGNORE (verrouille, cycle precedent deja applique) : delta=%.2f W", delta_error);
        // } 
        // else {
        //   ESP_LOGD(TAG, "Feed-Forward déclenché : Saut de %.2f W -> Ajustement sortie de %.4f", delta_error, pending_jump);
        // }
      }    
    }

    // ── Calcul PID ────────────────────────────────────────────────────
    tmp_i = this->error_ * this->dt_;
    if (!std::isnan(tmp_i)) this->integral_ += tmp_i;
    this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

    tmp = 0.0f;
    if (!std::isnan(this->previous_output_) && !this->current_pid_mode_) {
        tmp = this->previous_output_;
    }

    if(this->current_feedforward_){
        // ── Anti-répétition : le feedforward ne s'applique que si le verrou
        // n'est pas déjà posé (i.e. il n'a pas été appliqué au cycle précédent).
        if (!in_startup && !this->ff_locked_ && std::abs(pending_jump) > 0.001f) {
            tmp += pending_jump;
            // On s'assure que le saut manuel ne sort pas des limites globales
            tmp = std::min(std::max(tmp, this->output_min_), this->output_max_);
            this->ff_locked_ = true;   // verrouille le prochain cycle
        }
        else {
            // Rien appliqué ce cycle (pas de saut significatif, en startup,
            // ou verrou consommé) -> on relâche le verrou pour le cycle suivant
            this->ff_locked_ = false;
        }
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
        // Ne s'applique qu'après un arrêt réel (pass_through_==false) puisque
        // mode_start_time_ n'est réarmé que dans ce cas (voir plus bas).
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
    // pass_through_ est positionné ici selon la RAISON de la sortie de mode :
    //  - franchissement direct de la frontière opposée (rare) -> bascule
    //  - deadband + sortie déjà au repos                       -> arrêt réel
    //  - erreur franche + sortie déjà au repos                 -> bascule
    // olb_eff/oub_eff (hystérésis + marge adaptative) ne sont utilisés que
    // pour la détection de transition depuis IDLE (case 0).
    this->current_mode_ = this->previous_mode_;

    switch (this->previous_mode_) {
        case 0:  // IDLE
            if (this->current_output_ < olb_eff)
                this->current_mode_ = 1;
            else if (this->current_output_ > oub_eff)
                this->current_mode_ = 2;
            break;

        case 1:  // CHARGE
           // Sortie directe vers DISCHARGE si output franchit oub_ (rare mais possible)
           if (this->current_output_ > this->oub_) {
             this->current_mode_ = 2;
             this->pass_through_ = true;
           }
           // Arrêt réel : deadband confirmée + sortie déjà au minimum
           else if (this->current_deadband_ && (this->current_output_charging_ <= this->current_output_min_charging_ + 0.01f) && !in_startup) {
             this->current_mode_ = 0;
             this->pass_through_ = false;
           }
           // Bascule : erreur franchement positive + sortie déjà au minimum
           else if (!in_startup && (this->current_output_charging_ <= this->current_output_min_charging_ + 0.01f) && (epsi > this->Pmin_discharging * DEADBAND_FACTOR)) {
             this->current_mode_ = 0;   // → IDLE, qui basculera en DISCHARGE
             this->pass_through_ = true;
           }
           break;

        case 2:  // DISCHARGE
           if (this->current_output_ < this->olb_) {
             this->current_mode_ = 1;
             this->pass_through_ = true;
           }
           else if (this->current_deadband_ && (this->current_output_discharging_ <= this->current_output_min_discharging_ + 0.01f) && !in_startup) {
             this->current_mode_ = 0;
             this->pass_through_ = false;
           }
           else if (!in_startup && (this->current_output_discharging_ <= this->current_output_min_discharging_ + 0.01f) && (epsi < this->Pmin_charging * DEADBAND_FACTOR)) {
             this->current_mode_ = 0;   // → IDLE, qui basculera en CHARGE
             this->pass_through_ = true;
           }
           break;
     }

    // ── Transition de mode ────────────────────────────────────────────
    if (this->current_mode_ != this->previous_mode_) {

        this->record_transition_(now);   // ← anti-cyclage : enregistrer AVANT le switch

        if (this->current_mode_ == 1) {        // → CHARGE
            this->previous_output_ = this->olb_;
            this->current_output_  = this->olb_;
            if (!this->pass_through_) {
                // Arrêt réel précédent -> vrai redémarrage -> freeze complet
                this->mode_start_time_ = now;
            }
            // Si pass_through_ : le PCM était déjà alimenté (onoff_switch_ ON),
            // pas de freeze nécessaire, discharge_charge_switch_ va basculer
            // naturellement dans le bloc "Gestion discharge_charge_switch".
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
                // Arrêt réel : couper l'alimentation générale du PCM
                if ((this->onoff_switch_ != nullptr) && (this->onoff_switch_->state == true)) {
                    this->onoff_switch_->turn_off();
                    this->onoff_switch_->publish_state(false);
                    delay(ONOFF_DELAY);
                }
                // Repositionner discharge_charge_switch_ en "charge" par sécurité
                if (this->discharge_charge_switch_ != nullptr && this->discharge_charge_switch_->state == false) {
                    this->discharge_charge_switch_->turn_on();
                    this->discharge_charge_switch_->publish_state(true);
                    delay(CHARGE_DISCHARGE_DELAY);
                }
            }
            // Si pass_through_ : onoff_switch_ reste ON. discharge_charge_switch_
            // sera basculé par le bloc "Gestion discharge_charge_switch" dès que
            // output_charging_/discharging_ dépassera son minimum au prochain cycle.
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
    // C'est ici que la bascule électronique réelle a lieu, que ce soit après
    // un pass_through_ (onoff_switch_ resté ON) ou un redémarrage normal.
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
    if(this->current_output_charging_  > 0.0f){
      this->set_charging_level(this->current_output_charging_);
    }
    if(this->current_output_discharging_  > 0.0f){
      this->set_discharging_level(this->current_output_discharging_);
    }

    // ── Gestion onoff_switch ──────────────────────────────────────────
    // En pass_through_, onoff_switch_ est déjà ON depuis le mode précédent ;
    // ce bloc ne fait que confirmer/maintenir l'état cohérent avec la
    // consigne effective.
    if (this->onoff_switch_ != nullptr) {
        // En pass_through_, on garde onoff_switch_ allumé même pendant le
        // cycle transitoire en IDLE (output_charging_/discharging_ = 0),
        // sinon ce bloc le coupe avant même d'atteindre le mode opposé.
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

    ESP_LOGI(TAG, "out=%.4f Oc=%.4f Od=%.4f mode=%d deadband=%d startup=%d margin=%.3f pass_through=%d",
             this->current_output_,
             this->current_output_charging_,
             this->current_output_discharging_,
             this->previous_mode_,
             this->current_deadband_,
             (int)in_startup,
             this->adaptive_margin_,
             (int)this->pass_through_);

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
