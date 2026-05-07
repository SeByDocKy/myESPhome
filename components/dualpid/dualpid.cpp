#include "dualpid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpid {

// Facteur zone morte : |epsi| < Pmin * DEADBAND_FACTOR → on ne fait rien

#define DEADBAND_FACTOR  1.03f
#define HMS_MIN_LEVEL  0.02f
#define STARTUP_INHIBIT_MS  6000

static const char *const TAG = "dualpid";

static const float coeffPcharging = 0.0001f;
static const float coeffIcharging = 0.0001f;
static const float coeffDcharging = 0.001f;

static const float coeffPdischarging = 0.001f;
static const float coeffIdischarging = 0.0001f;
static const float coeffDdischarging = 0.001f;


// ── Helpers d'envoi de consignes (n'envoient que si valeur change) ────────────

void DUALPIDComponent::set_charging_level(float level) {
    // Comparer avec previous pour détecter un vrai changement depuis
    // le dernier cycle envoyé — current peut avoir été modifié manuellement
    if (level != this->previous_output_charging_) {
        this->device_charging_output_->set_level(level);
        ESP_LOGD(TAG, "set_charging_level: %.4f", level);
    }
    // Toujours mettre à jour current et previous
    this->current_output_charging_  = level;
    this->previous_output_charging_ = level;
}

void DUALPIDComponent::set_discharging_level(float level) {
    if (level != this->previous_output_discharging_) {
        if (level <= HMS_MIN_LEVEL) {
            this->device_discharging_output_->set_level(level);
            ESP_LOGD(TAG, "set_discharging_level (min/stop): %.4f", level);
        } else {
            if ((this->producing_binary_sensor_ == nullptr)
                || (this->producing_binary_sensor_->state == true)) {
                this->device_discharging_output_->set_level(level);
                ESP_LOGD(TAG, "set_discharging_level: %.4f", level);
            }
        }
    }
    this->current_output_discharging_  = level;
    this->previous_output_discharging_ = level;
}






// void DUALPIDComponent::set_charging_level(float level) {
//     if (level != this->current_output_charging_) {
//         this->device_charging_output_->set_level(level);
//         this->current_output_charging_ = level;
//         ESP_LOGD(TAG, "set_charging_level: %.4f", level);
//     }
// }

// void DUALPIDComponent::set_discharging_level(float level) {
//     if (level != this->current_output_discharging_) {
//         if (level <= HMS_MIN_LEVEL) {
//             // Retour au niveau minimum : toujours envoyer (sécurité)
//             this->device_discharging_output_->set_level(level);
//             this->current_output_discharging_ = level;
//             ESP_LOGD(TAG, "set_discharging_level (min/stop): %.4f", level);
//         } else {
//             // Consigne active : envoyer seulement si HMS prêt à produire
//             if ((this->producing_binary_sensor_ == nullptr)
//                 || (this->producing_binary_sensor_->state == true)) {
//                 this->device_discharging_output_->set_level(level);
//                 this->current_output_discharging_ = level;
//                 ESP_LOGD(TAG, "set_discharging_level: %.4f", level);
//             }
//         }
//     }
// }


// ── Setup ─────────────────────────────────────────────────────────────────────

void DUALPIDComponent::setup() {
    ESP_LOGCONFIG(TAG, "Setting up DUALPIDComponent...");

    this->last_time_                   = millis();
    this->integral_                    = 0.0f;
    this->previous_error_              = 0.0f;
    this->previous_output_             = this->current_epoint_;
    this->previous_output_charging_    = 0.0f;
    this->previous_output_discharging_ = HMS_MIN_LEVEL;
    this->previous_activation_         = false;
    this->current_mode_                = 0;     // 0=IDLE, 1=CHARGE, 2=DISCHARGE
    this->previous_mode_               = 0;
    this->mode_start_time_             = millis() - STARTUP_INHIBIT_MS;

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

    // S'assurer que le r48 est bien arrêté avant (sécurité)
    if (this->r48_general_switch_ != nullptr && this->r48_general_switch_->state == true) {
        this->r48_general_switch_->turn_off();
        this->r48_general_switch_->publish_state(false);
    }

    this->pid_computed_callback_.call();

    ESP_LOGI(TAG, "setup: battery_voltage=%3.2f, pid_mode = %d",
             this->current_battery_voltage_, this->current_pid_mode_);
}


void DUALPIDComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "dump config:");
    ESP_LOGVV(TAG, "setup import part: battery_voltage=%3.2f", this->current_battery_voltage_);
    this->pid_computed_callback_.call();
}


// ── pid_update ────────────────────────────────────────────────────────────────

void DUALPIDComponent::pid_update() {
    uint32_t now = millis();
    float tmp, tmp_i, epsi;
    float alphaP, alphaI, alphaD, alpha;
    float coeffP, coeffI, coeffD;
    bool raw_deadband;
    bool in_startup;
    float Pmin_ch, Pmin_dis;
    float elb, eub;
    float o_min_charge, o_max_charge, o_min_discharge, o_max_discharge, o_clamped, span;
    float span_c, oc, span_d, od;

    ESP_LOGI(TAG, "Entered in pid_update()");
    ESP_LOGI(TAG, "Current pid mode %d", this->current_pid_mode_);

    this->pid_computed_callback_.call();

    if (this->current_manual_override_) {
        return;
    }
    else {

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
    }
    else if (this->current_battery_voltage_ < this->current_charged_battery_voltage_) {
        this->current_epoint_ = this->current_absorbing_epoint_;
    }
    else {
        this->current_epoint_ = this->current_floating_epoint_;
    }

    // Bornes d'hystérésis autour de epoint_
    elb = this->current_epoint_ - this->o_hysteresis_;
    elb = std::min(std::max(elb, this->current_output_min_), this->current_output_max_);
    eub = this->current_epoint_ + this->o_hysteresis_;
    eub = std::min(std::max(eub, this->current_output_min_), this->current_output_max_);

    // ── Calcul de l'erreur ────────────────────────────────────────────
    epsi         = this->current_input_ - this->current_setpoint_;
    this->error_ = epsi;
    if (this->current_reverse_) this->error_ = -this->error_;
    this->current_error_ = this->error_;

    // ── Reset propre au passage activation off → on ───────────────────
    if (this->current_activation_ && !this->previous_activation_) {
        this->previous_error_ = this->error_;
        this->integral_       = 0.0f;
        this->previous_mode_  = 0;
        this->current_mode_   = 0;
        if (this->error_ > 0.0f) {
            this->previous_output_ = eub;
            this->current_output_  = eub;
        }
        else {
            this->previous_output_ = elb;
            this->current_output_  = elb;
        }
    }

    this->previous_activation_ = this->current_activation_;

    // ── Bloc désactivation ────────────────────────────────────────────
    if (!this->current_activation_) {
        // ─────────────────────── transition activation on à off ───────────────────────
        if (this->current_output_discharging_ > this->current_output_min_discharging_) {
            this->set_discharging_level(HMS_MIN_LEVEL);
        }
        if (this->current_output_charging_ > this->current_output_min_charging_) {
            this->set_charging_level(0.0f);
        }

        this->output_charging_             = 0.0f;
        this->output_discharging_          = HMS_MIN_LEVEL;
        this->current_output_charging_     = 0.0f;
        this->current_output_discharging_  = HMS_MIN_LEVEL;
        this->previous_output_charging_    = 0.0f;
        this->previous_output_discharging_ = HMS_MIN_LEVEL;
        this->current_output_              = this->current_epoint_;
        this->previous_output_             = this->current_epoint_;
        this->previous_mode_               = 0;
        this->current_mode_                = 0;
        this->last_time_                   = now;
        // ─────────────────────── Arrêt de l'emerson R48  ─────────────────────── 
        if (this->r48_general_switch_ != nullptr && this->r48_general_switch_->state == true) {
            this->r48_general_switch_->turn_off();
            this->r48_general_switch_->publish_state(false);
        }

        this->pid_computed_callback_.call();
        return;
    }

    // ── Protection sous-tension batterie ─────────────────────────────
    if (!std::isnan(this->current_battery_voltage_)) {
        ESP_LOGI(TAG, "battery_voltage=%.2f, starting=%.2f",
                 this->current_battery_voltage_, this->current_starting_battery_voltage_);
        if (this->current_battery_voltage_ < this->current_starting_battery_voltage_) {
            this->set_charging_level(0.0f);
            this->set_discharging_level(HMS_MIN_LEVEL);
            this->output_charging_             = 0.0f;
            this->output_discharging_          = HMS_MIN_LEVEL;
            this->current_output_              = this->current_epoint_;
            this->previous_output_             = this->current_epoint_;
            this->previous_mode_               = 0;
            this->current_mode_                = 0;
            this->current_output_charging_     = 0.0f;
            this->current_output_discharging_  = HMS_MIN_LEVEL;
            this->previous_output_charging_    = 0.0f;
            this->previous_output_discharging_ = HMS_MIN_LEVEL;
            this->last_time_                   = now;
            this->pid_computed_callback_.call();
            return;
        }
    }

    // ── Calcul deadband asymétrique ───────────────────────────────────
    //
    // R48 (charge)  : courant minimum = 3.5A
    //   → Pmin_charging = Vbatt × Imin_charge  (ex: 51.2 × 3.5 ≈ 179 W)
    //   → deadband côté charge : epsi > -Pmin_charging × DEADBAND_FACTOR
    //
    // HMS (décharge) : courant minimum ≈ 0A
    //   → Pmin_discharging ≈ 0 W
    //   → pas de deadband côté décharge
    //
    // Zone morte ASYMÉTRIQUE :
    //   epsi < -Pmin_ch × k    → surplus suffisant → CHARGE
    //   epsi > +Pmin_dis × k   → moindre conso → DISCHARGE
    //   entre les deux         → IDLE

    Pmin_ch  = this->current_battery_voltage_ * this->current_min_charging_;
    Pmin_dis = this->current_battery_voltage_ * this->current_min_discharging_;
        
   if (this->current_activation_){
      raw_deadband = (epsi > -(Pmin_ch * DEADBAND_FACTOR)) && (epsi <  (Pmin_dis * DEADBAND_FACTOR));

    // ── Fix ii) : deadband basée uniquement sur raw_deadband + in_startup ──
    // Suppression de output_is_active : il causait une sortie prématurée de
    // CHARGE quand output_charging descendait à output_min_charging alors que
    // error était encore < 0. La protection contre le démarrage prématuré
    // est assurée par in_startup seul, et la sortie de mode par la machine
    // d'état (condition error > Pmin_ch dans case 1 et case 2).
      in_startup = (now - this->mode_start_time_) < STARTUP_INHIBIT_MS;
      this->current_deadband_ = raw_deadband && !in_startup;
   }
   else{
    this->current_deadband_ = false;
   }

    ESP_LOGI(TAG, "deadband: epsi=%.1f Pmin_ch=%.1f Pmin_dis=%.1f raw=%d startup=%d db=%d",
             epsi, Pmin_ch, Pmin_dis, raw_deadband, (int)in_startup, this->current_deadband_);

    // ── Deadband en mode IDLE : on reste off ──────────────────────────
    if (this->current_deadband_ && this->previous_mode_ == 0) {
        this->output_charging_             = 0.0f;
        this->output_discharging_          = HMS_MIN_LEVEL;
        this->last_time_                   = now;
        this->previous_error_              = this->error_;
        this->previous_output_charging_    = 0.0f;
        this->previous_output_discharging_ = HMS_MIN_LEVEL;
        this->pid_computed_callback_.call();
        return;
    }

    // ── Deadband depuis mode ACTIF : arrêt immédiat ───────────────────
    if (this->current_deadband_ && this->previous_mode_ != 0 && this->current_activation_) {
        this->output_charging_            = 0.0f;
        this->output_discharging_         = HMS_MIN_LEVEL;
        this->current_output_charging_    = 0.0f;
        this->current_output_discharging_ = HMS_MIN_LEVEL;

        // Repartir à la frontière du mode quitté pour redémarrer vite
        if (this->previous_mode_ == 2) {
            this->previous_output_ = eub;
            this->current_output_  = eub;
        }
        else {
            this->previous_output_ = elb;
            this->current_output_  = elb;
        }
        this->previous_mode_ = 0;
        this->current_mode_  = 0;
        // --- arrêt de l'emerson 48 ---
        if (this->r48_general_switch_ != nullptr && this->r48_general_switch_->state == true) {
            this->r48_general_switch_->turn_off();
            this->r48_general_switch_->publish_state(false);
        }
        this->set_charging_level(0.0f);
        this->set_discharging_level(HMS_MIN_LEVEL);

        this->last_time_                   = now;
        this->previous_error_              = this->error_;
        this->previous_output_charging_    = 0.0f;
        this->previous_output_discharging_ = HMS_MIN_LEVEL;
        this->pid_computed_callback_.call();
        return;
    }

    // ── PID — coefficients selon le mode courant ──────────────────────
    if (this->previous_mode_ == 1) {   // CHARGE
        coeffP = coeffPcharging    * this->current_kp_charging_;
        coeffI = coeffIcharging    * this->current_ki_charging_;
        coeffD = coeffDcharging    * this->current_kd_charging_;
    }
    else {                             // DISCHARGE ou IDLE → gains décharge
        coeffP = coeffPdischarging * this->current_kp_discharging_;
        coeffI = coeffIdischarging * this->current_ki_discharging_;
        coeffD = coeffDdischarging * this->current_kd_discharging_;
    }

    tmp_i = this->error_ * this->dt_;
    if (!std::isnan(tmp_i) && (this->previous_mode_ != 0)) this->integral_ += tmp_i;
    this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;

    tmp = 0.0f;
    if (!std::isnan(this->previous_output_) && !this->current_pid_mode_) {
        tmp = this->previous_output_;
    }

    alphaP = coeffP * this->error_;
    alphaI = coeffI * this->integral_;
    alphaD = coeffD * this->derivative_;
    alpha  = alphaP + alphaI + alphaD;

    this->current_output_ = std::min(std::max(tmp + alpha,
                                               this->current_output_min_),
                                     this->current_output_max_);

    if (this->previous_mode_ == 1) {        // CHARGE — output ∈ [0, elb]
        o_min_charge = elb * (1.0f - this->current_output_max_charging_);
        o_max_charge = elb * (1.0f - this->current_output_min_charging_);
        o_clamped    = std::min(std::max(this->current_output_, o_min_charge), o_max_charge);
        if ((o_clamped != this->current_output_) && (tmp_i < 0.0f)) {
            this->integral_ -= tmp_i;
        }
        this->current_output_ = o_clamped;

        // ── Freeze pendant démarrage Emerson ──────────────────────────
        // Bloque output à elb (= Oc=0) le temps que l'Emerson soit opérationnel.
        // Remet previous_error_ à jour pour éviter un pic dérivé au déblocage.
        if (in_startup) {
            this->current_output_  = elb;
            this->integral_        = 0.0f;
            this->previous_output_ = elb;
            this->previous_error_  = this->error_;
        }
    }
    else if (this->previous_mode_ == 2) {   // DISCHARGE — output ∈ [eub, 1]
        span            = 1.0f - eub;
        o_min_discharge = eub + this->current_output_min_discharging_ * span;
        o_max_discharge = eub + this->current_output_max_discharging_ * span;
        o_clamped       = std::min(std::max(this->current_output_, o_min_discharge), o_max_discharge);
        if ((o_clamped != this->current_output_) && (tmp_i > 0.0f)) {
            this->integral_ -= tmp_i;
        }
        this->current_output_ = o_clamped;
    }

    // Mode IDLE (0) : output contraint entre elb et eub
    // pour éviter les transitions sur bruit de mesure
    if (this->previous_mode_ == 0) {
        this->current_output_ = std::min(std::max(this->current_output_, elb), eub);
    }

    ESP_LOGI(TAG, "PID: E=%.2f I=%.2f D=%.2f alpha=%.6f prev=%.4f out=%.4f",
             this->error_, this->integral_, this->derivative_, alpha, tmp, this->current_output_);

    // ── Machine d'état ────────────────────────────────────────────────
    this->current_mode_ = this->previous_mode_;

    switch (this->previous_mode_) {
        case 0:  // IDLE
            if (this->current_output_ <= elb)
                this->current_mode_ = 1;   // → CHARGE
            else if (this->current_output_ >= eub)
                this->current_mode_ = 2;   // → DISCHARGE
            break;

        case 1:  // CHARGE
            if (this->current_deadband_) {
                this->current_mode_ = 0;
            }
            // error franchement positif → surplus insuffisant → IDLE
            else if (!in_startup && this->error_ > (Pmin_ch * DEADBAND_FACTOR)) {
                this->current_mode_ = 0;   // → IDLE, qui basculera en DISCHARGE
            }
            break;

        case 2:  // DISCHARGE
            if (this->current_deadband_) {
                this->current_mode_ = 0;
            }
            // error franchement négatif → surplus suffisant → IDLE
            else if (!in_startup && this->error_ < -(Pmin_ch * DEADBAND_FACTOR)) {
                this->current_mode_ = 0;   // → IDLE, qui basculera en CHARGE
            }
            break;
    }

    // ── Transition de mode ────────────────────────────────────────────
    if (this->current_mode_ != this->previous_mode_ && this->current_activation_) {

        if (this->current_mode_ == 1) {        // → CHARGE
            if ((this->r48_general_switch_ != nullptr)
                && (this->r48_general_switch_->state == false)) {
                this->r48_general_switch_->turn_on();
                this->r48_general_switch_->publish_state(true);
            }
            this->mode_start_time_             = now;
            this->integral_                    = 0.0f;
            this->output_charging_             = 0.0f;
            this->current_output_charging_     = 0.0f;
            this->set_charging_level(0.0f);
            this->output_discharging_          = HMS_MIN_LEVEL;
            this->previous_output_discharging_ = HMS_MIN_LEVEL;
            this->set_discharging_level(HMS_MIN_LEVEL);
            this->previous_output_             = elb;
            this->current_output_              = elb;
        }
        else if (this->current_mode_ == 2) {   // → DISCHARGE
            if ((this->r48_general_switch_ != nullptr) && (this->r48_general_switch_->state == true)) {
                this->r48_general_switch_->turn_off();
                this->r48_general_switch_->publish_state(false);
            }
            this->output_charging_         = 0.0f;
            this->current_output_charging_ = 0.0f;
            this->set_charging_level(0.0f);
            this->previous_output_         = eub;
            this->current_output_          = eub;
            this->integral_                = 0.0f;
            this->mode_start_time_         = now;
        }
        else {                                  // → IDLE
            this->integral_ = 0.0f;
            if (this->error_ < 0.0f) {
                this->previous_output_ = elb;
                this->current_output_  = elb;
            }
            else {
                this->previous_output_ = eub;
                this->current_output_  = eub;
            }
            this->output_charging_            = 0.0f;
            this->output_discharging_         = HMS_MIN_LEVEL;
            this->current_output_charging_    = 0.0f;
            this->current_output_discharging_ = HMS_MIN_LEVEL;
            this->set_charging_level(0.0f);
            this->set_discharging_level(HMS_MIN_LEVEL);
        }

        this->previous_mode_               = this->current_mode_;
        this->last_time_                   = now;
        this->previous_error_              = this->error_;
        this->previous_output_charging_    = 0.0f;
        this->previous_output_discharging_ = HMS_MIN_LEVEL;
        this->pid_computed_callback_.call();
        return;
    }

    // ── Calcul des sorties physiques selon le mode ────────────────────
    switch (this->previous_mode_) {

        case 0:  // IDLE
            this->output_charging_    = 0.0f;
            this->output_discharging_ = HMS_MIN_LEVEL;
            break;

        case 1: {  // CHARGE — output ∈ [0, elb] → Oc ∈ [Ocmax, 0]
            // output = elb → Oc = 0,  output = 0 → Oc = max
            if (this->output_discharging_ > HMS_MIN_LEVEL && this->current_activation_) {
                this->set_discharging_level(HMS_MIN_LEVEL);
                this->output_discharging_ = HMS_MIN_LEVEL;
            }
            span_c                    = (elb > 0.0f) ? elb : 1.0f;
            oc                        = (elb - this->current_output_) / span_c;
            this->output_charging_    = std::min(std::max(oc, this->current_output_min_charging_), this->current_output_max_charging_);
            this->output_discharging_ = HMS_MIN_LEVEL;
            break;
        }

        case 2: {  // DISCHARGE — output ∈ [eub, 1] → Od ∈ [0, Odmax]
            // Sécurité : forcer le R48 à 0 si encore actif
            if (this->output_charging_ > 0.0f && this->current_activation_) {
                this->set_charging_level(0.0f);
                this->output_charging_ = 0.0f;
            }
            span_d                    = (1.0f - eub > 0.0f) ? (1.0f - eub) : 1.0f;
            od                        = (this->current_output_ - eub) / span_d;
            this->output_charging_    = 0.0f;
            this->output_discharging_ = std::min(std::max(od, this->current_output_min_discharging_), this->current_output_max_discharging_);
            break;
        }
    }

    // ── Commutation r48 selon la sortie effective (filet de sécurité) ─
    if (this->r48_general_switch_ != nullptr) {
        if ((this->output_charging_ > this->current_output_min_charging_) && !this->r48_general_switch_->state) {
            this->r48_general_switch_->turn_on();
            this->r48_general_switch_->publish_state(true);
            ESP_LOGI(TAG, "r48 turned ON (charge safety net)");
        }
        else if ((this->previous_mode_ == 2) && this->r48_general_switch_->state) {
            this->r48_general_switch_->turn_off();
            this->r48_general_switch_->publish_state(false);
            ESP_LOGI(TAG, "r48 turned OFF (discharge safety net)");
        }
    }

    // ── Envoi des consignes via les helpers (fix i) ───────────────────
    // set_charging_level et set_discharging_level n'appellent set_level
    // que si la valeur a changé → évite de saturer le canal radio HMS/OpenDTU
    if (this->current_activation_) {
        this->set_charging_level(this->output_charging_);
        this->set_discharging_level(this->output_discharging_);
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
    // this->current_output_charging_     = this->output_charging_;
    // this->current_output_discharging_  = this->output_discharging_;
    // this->previous_output_charging_    = this->output_charging_;
    // this->previous_output_discharging_ = this->output_discharging_;

    this->pid_computed_callback_.call();
    }  // end else !manual_override
}

}  // namespace dualpid
}  // namespace esphome
