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


 SUB_NUMBER(setpoint)
 SUB_NUMBER(feedforward_threshold)
 SUB_NUMBER(starting_battery_voltage)
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
  void set_feedforward_threshold(float thresh){this->current_feedforward_threshold_ = thresh;}
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

  void set_setpoint(float value) {this->current_setpoint_ = value;}
  float get_setpoint(void){return this->current_setpoint_;}

  void set_feedforward_threshold(float value) {this->current_feedforward_threshold_ = value;}
  float get_feedforward_threshold(void){return this->current_feedforward_threshold_;}
  
  void set_starting_battery_voltage(float value) {this->current_starting_battery_voltage_ = value;}
  float get_starting_battery_voltage(void){return this->current_starting_battery_voltage_;}

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
  bool get_deadband(void){return this->current_deadband_;}

  // ── Anti-cyclage adaptatif ────────────────────────────────────────────────
  float get_adaptive_margin(void){return this->adaptive_margin_;}

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
  // float current_feedforward_threshold_ = 300.0f;

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
  
    

  // ── Anti-cyclage adaptatif ────────────────────────────────────────────────
  // Historique des N dernières transitions de mode (IDLE<->CHARGE/DISCHARGE).
  // Si N transitions se produisent dans une fenêtre glissante trop courte,
  // on élargit temporairement l'hystérésis effective (olb_eff/oub_eff)
  // pour freiner le cyclage. La marge se relâche automatiquement après une
  // période de calme.
  static const uint8_t TRANSITION_HISTORY_SIZE = 4;
  uint32_t transition_history_[TRANSITION_HISTORY_SIZE] = {0, 0, 0, 0};
  uint8_t  transition_idx_   = 0;
  float    adaptive_margin_  = 0.0f;   // marge additionnelle courante (0 = pas de cyclage détecté)

  void record_transition_(uint32_t now);
  void decay_adaptive_margin_(uint32_t now);

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
