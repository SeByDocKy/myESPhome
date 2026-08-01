#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/time/real_time_clock.h"


namespace esphome {
namespace dualpidpcm {
	
class DUALPIDPCMComponent : public Component{


 SUB_SWITCH(activation)
 SUB_SWITCH(manual_override)
 SUB_SWITCH(pid_mode)
 SUB_SWITCH(reverse)
 SUB_SWITCH(feedforward)
 SUB_SWITCH(allow_charging)
 SUB_SWITCH(allow_discharging)


 

 SUB_NUMBER(setpoint)
 SUB_NUMBER(feedforward_threshold)
 SUB_NUMBER(self_consumption)
 SUB_NUMBER(starting_battery_voltage)
 SUB_NUMBER(stopping_battery_voltage)
 SUB_NUMBER(discharge_self_consumption)
 // ── Hystérésis start/stop (anti-chatter) ──────────────────────────────────
 // Écart en W à ajouter au seuil d'arrêt (Pmin_charging/Pmin_discharging)
 // pour obtenir le seuil de (re)démarrage. self_consumption/discharge_self_
 // consumption positionnent le seuil d'ARRÊT (motivation physique) ; ces deux
 // numbers positionnent l'écart avant REDÉMARRAGE (motivation anti-cyclage).
 SUB_NUMBER(delta_idle_charging)
 SUB_NUMBER(delta_idle_discharging)
 SUB_NUMBER(kp)
 SUB_NUMBER(ki)
 SUB_NUMBER(kd)

 SUB_NUMBER(output_min_charging)
 SUB_NUMBER(output_max_charging)
 SUB_NUMBER(output_min_discharging)
 SUB_NUMBER(output_max_discharging)

 public:
  
  void setup() override;
  void dump_config() override;
  
  void set_input_sensor(sensor::Sensor *input_sensor) {this->input_sensor_ = input_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) {this->battery_voltage_sensor_ = battery_voltage_sensor; }
  void set_device_charging_output(output::FloatOutput *output) {this->device_charging_output_ = output; }
  void set_device_discharging_output(output::FloatOutput *output) {this->device_discharging_output_ = output; }
  void set_discharge_charge_switch(switch_::Switch *sw) {this->discharge_charge_switch_ = sw;}
  void set_onoff_switch(switch_::Switch *sw) {this->onoff_switch_ = sw;}
  void set_current_min_charging_register(float current){this->current_min_charging_ = current;}
  void set_current_min_discharging_register(float current){this->current_min_discharging_ = current;}
  void set_charging_level(float level);
  void set_discharging_level(float level);
   
  void pid_update();
  
  void add_on_pid_computed_callback(std::function<void()> &&callback) {
    pid_computed_callback_.add(std::move(callback));
  }

  float O_to_Oc(float O);
  float O_to_Od(float O);
  float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v);};
  float calculate_ff_jump(float delta_w);

  
  void set_activation(bool enable) {this->current_activation_ = enable;}
  bool get_activation(void){return this->current_activation_;}
  void set_manual_override(bool enable) {this->current_manual_override_ = enable;}
  bool get_manual_override(void){return this->current_manual_override_;}
  void set_pid_mode(bool enable) {this->current_pid_mode_ = enable;}
  bool get_pid_mode(void){return this->current_pid_mode_;}
  void set_reverse(bool enable) {this->current_reverse_ = enable;}
  bool get_reverse(void){return this->current_reverse_;}
  void set_feedforward(bool enable) {this->current_feedforward_ = enable;}
  bool get_feedforward(void){return this->current_feedforward_;}
  void set_allow_charging(bool enable) {this->current_allow_charging_ = enable;}
  bool get_allow_charging(void){return this->current_allow_charging_;}
  void set_allow_discharging(bool enable) {this->current_allow_discharging_ = enable;}
  bool get_allow_discharging(void){return this->current_allow_discharging_;}

  void set_setpoint(float value) {this->current_setpoint_ = value;}
  float get_setpoint(void){return this->current_setpoint_;}

  void set_feedforward_threshold(float value) {this->current_feedforward_threshold_ = value;}
  float get_feedforward_threshold(void){return this->current_feedforward_threshold_;}
  
  void set_starting_battery_voltage(float value) {this->current_starting_battery_voltage_ = value;}
  float get_starting_battery_voltage(void){return this->current_starting_battery_voltage_;}

  void set_stopping_battery_voltage(float value) {this->current_stopping_battery_voltage_ = value;}
  float get_stopping_battery_voltage(void){return this->current_stopping_battery_voltage_;}

  // Autoconsommation à vide du convertisseur en décharge (W). Vient s'ajouter
  // à la consommation mesurée de la maison pour déterminer le seuil réel
  // Pmin_discharging (seuil d'ARRÊT) à partir duquel décharger devient
  // réellement utile.
  void set_self_consumption(float value) {this->current_self_consumption_ = value;}
  float get_self_consumption(void){return this->current_self_consumption_;}

  // Écart (W) entre le seuil d'ARRÊT et le seuil de REDÉMARRAGE, par direction.
  // Pstart_charging    = Pmin_charging    - delta_idle_charging_
  // Pstart_discharging = Pmin_discharging + delta_idle_discharging_
  void set_delta_idle_charging(float value) {this->current_delta_idle_charging_ = value;}
  float get_delta_idle_charging(void){return this->current_delta_idle_charging_;}
  void set_delta_idle_discharging(float value) {this->current_delta_idle_discharging_ = value;}
  float get_delta_idle_discharging(void){return this->current_delta_idle_discharging_;}

  void set_kp(float value) {this->current_kp_ = value;}
  float get_kp(void){return this->current_kp_;}
  void set_ki(float value) {this->current_ki_ = value;}
  float get_ki(void){return this->current_ki_;}
  void set_kd(float value) {this->current_kd_ = value;}
  float get_kd(void){return this->current_kd_;}
    
  void set_output_min_charging(float value) {this->current_output_min_charging_ = value;}
  float get_output_min_charging(void){return this->current_output_min_charging_;}
  void set_output_max_charging(float value) {this->current_output_max_charging_ = value;}
  float get_output_max_charging(void){return this->current_output_max_charging_;}

  void set_output_min_discharging(float value) {this->current_output_min_discharging_ = value;}
  float get_output_min_discharging(void){return this->current_output_min_discharging_;}
  void set_output_max_discharging(float value) {this->current_output_max_discharging_ = value;}
  float get_output_max_discharging(void){return this->current_output_max_discharging_;}
  
  float get_error(void) { return this->current_error_; }
  float get_output(void) { return this->current_output_; }
  float get_output_charging(void) { return this->current_output_charging_; }
  float get_output_discharging(void) { return this->current_output_discharging_; }  
  float get_input(void)  { return this->current_input_; }

  float get_mode(void) {return this->current_mode_;}
  // Exposé stable pour le binary_sensor : reflète "actif ET en IDLE"
  // (previous_mode_ == 0 && current_activation_), qui ne change QUE lors
  // d'une vraie transition de mode hystérétique — contrairement au calcul
  // brut de current_deadband_ (basé sur les seuils STOP), recalculé à
  // chaque cycle indépendamment du mode et donc sujet au yoyo quand epsi
  // oscille près de ces seuils. current_deadband_ reste utilisé en interne
  // pour piloter la sortie réelle de CHARGE/DISCHARGE (inchangé).
  //
  // IMPORTANT : le && current_activation_ est indispensable — sans lui,
  // le binary_sensor affiche "deadband=true" en continu tant que le
  // composant n'est pas activé (previous_mode_ reste forcé à 0 dans le
  // bloc désactivation), quelle que soit l'erreur réelle. C'est exactement
  // ce que faisait déjà current_deadband_ (mis à false explicitement dans
  // ce même bloc) — ce getter doit préserver la même garantie.
  bool get_deadband(void){return (this->previous_mode_ == 0) && this->current_activation_;}

  // ── Bascule directe CHARGE<->DISCHARGE sans coupure onoff_switch_ ─────────
  bool get_pass_through(void){return this->pass_through_;}
  

 protected:
  uint32_t last_time_ = 0;
  float dt_;
  float error_ = 0.0f;
  float previous_error_ = 0.0f;

  float previous_output_ = 0.5f;
  float previous_output_charging_ = 0.0f;
  float previous_output_discharging_ = 0.0f;

  float integral_= 0.0f; 
  float derivative_ = 0.0f;
  float current_min_charging_ = 5.0f;
  float current_min_discharging_ = 5.0f;

  float Pmin_charging = 1.0f*51.2f;
  float Pmin_discharging = 1.0f*51.2f;
  float Pdeadband_ = 1.5f*51.2f;

  
  float current_battery_voltage_ = 54.0f;
  float current_device_output_charging_ = 0.0f;
  float current_device_output_discharging_ = 0.0f;
  
  sensor::Sensor *input_sensor_;
  sensor::Sensor *battery_voltage_sensor_;
  output::FloatOutput *device_charging_output_; 
  output::FloatOutput *device_discharging_output_;
  switch_::Switch  *discharge_charge_switch_;
  switch_::Switch  *onoff_switch_;
    
  CallbackManager<void()> pid_computed_callback_;

  float current_error_ = 0.0f;
  float current_output_charging_ = 0.0f;
  float current_output_discharging_ = 0.0f;
  float current_input_ = 0.0f;
  float current_output_ = 0.5f;
 
  bool current_activation_ = false;
  bool current_manual_override_ = false;
  bool current_pid_mode_ = false;
  bool current_reverse_ = false;
  bool current_feedforward_ = false;


  float current_setpoint_ = 0.0f;
  float current_feedforward_threshold_ = 300.0f;
  float current_starting_battery_voltage_ = 51.0f;

  // Seuil bas de l'hystérésis de sous-tension : en dessous, on arrête
  // (undervoltage_lockout_ = true) ; il faut remonter au-dessus de
  // current_starting_battery_voltage_ pour relancer.
  float current_stopping_battery_voltage_ = 49.5f;
  bool  undervoltage_lockout_ = false;

  // Autoconsommation à vide du convertisseur en décharge (W) — seuil d'ARRÊT.
  float current_self_consumption_ = 35.0f;

  // Écart (W) seuil d'ARRÊT -> seuil de REDÉMARRAGE, par direction.
  float current_delta_idle_charging_    = 30.0f;
  float current_delta_idle_discharging_ = 30.0f;

  float current_kp_          = 1.1f;
  float current_ki_          = 0.0f;
  float current_kd_          = 0.0f;
     
  float current_output_max_charging_ = 1.0f;
  float current_output_min_charging_ = 0.0f;

  float current_output_max_discharging_ = 1.0f;
  float current_output_min_discharging_ = 0.0f;

  bool current_deadband_                = false;
 
  uint32_t mode_start_time_             = 0;
 

  float lb_             = 0.01f;
  float ub_             = 0.01f;
  float oneutral_       = 0.5f;
  float olb_;
  float oub_;

  float output_min_      = 0.0f;
  float output_max_      = 1.0f;

  float o_hysteresis_    = 0.02f;

  int previous_mode_     = 0;
  int current_mode_      = 0; // 0 <=> idle, 1<-> charge, 2 <-> discharge

  bool current_onoff_    = false; 
  bool previous_activation_ = false;

  bool current_allow_charging_ = true;
  bool current_allow_discharging_ = true;


  // ── Bascule directe CHARGE<->DISCHARGE ────────────────────────────────────
  // Le PCM gère électroniquement le sens (discharge_charge_switch_) sans
  // nécessiter de coupure de l'alimentation générale (onoff_switch_).
  // pass_through_ = true  : on quitte CHARGE/DISCHARGE parce que l'on bascule
  //                         directement vers l'autre mode -> onoff_switch_
  //                         reste allumé, seul discharge_charge_switch_ change.
  // pass_through_ = false : arrêt réel (deadband) -> onoff_switch_ est coupé
  //                         et le prochain démarrage repasse par le freeze
  //                         STARTUP_INHIBIT_MS.
  bool pass_through_ = false;
  
  // typedef enum {
  //   MODE_IDLE,       // Ni charge, ni décharge (zone morte)
  //   MODE_CHARGE,     // Chargement batterie  (O ∈ [0.0 – 0.5[)
  //   MODE_DISCHARGE   // Décharge batterie    (O ∈ ]0.5 – 1.0])
  // } ConverterMode_;

};
		
 }  // namespace dualpidpcm
}  // namespace esphome
