#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/version.h"

namespace esphome {
namespace victron_smart_shunt {

class VictronSmartShuntComponent : public uart::UARTDevice, public Component {
 public:
  // MPPT
  void set_max_power_yesterday_sensor(sensor::Sensor *max_power_yesterday_sensor) {
    max_power_yesterday_sensor_ = max_power_yesterday_sensor;
  }
  void set_max_power_today_sensor(sensor::Sensor *max_power_today_sensor) {
    max_power_today_sensor_ = max_power_today_sensor;
  }
  void set_yield_total_sensor(sensor::Sensor *yield_total_sensor) { yield_total_sensor_ = yield_total_sensor; }
  void set_yield_yesterday_sensor(sensor::Sensor *yield_yesterday_sensor) {
    yield_yesterday_sensor_ = yield_yesterday_sensor;
  }
  void set_yield_today_sensor(sensor::Sensor *yield_today_sensor) { yield_today_sensor_ = yield_today_sensor; }
  void set_panel_voltage_sensor(sensor::Sensor *panel_voltage_sensor) { panel_voltage_sensor_ = panel_voltage_sensor; }
  void set_panel_power_sensor(sensor::Sensor *panel_power_sensor) { panel_power_sensor_ = panel_power_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) {
    battery_voltage_sensor_ = battery_voltage_sensor;
  }  // MPPT + SS
  void set_battery_current_sensor(sensor::Sensor *battery_current_sensor) {
    battery_current_sensor_ = battery_current_sensor;
  }  // MPPT + SS
  void set_load_current_sensor(sensor::Sensor *load_current_sensor) { load_current_sensor_ = load_current_sensor; }
  void set_day_number_sensor(sensor::Sensor *day_number_sensor) { day_number_sensor_ = day_number_sensor; }
  void set_charger_status_sensor(sensor::Sensor *charger_status_sensor) {
    charger_status_sensor_ = charger_status_sensor;
  }
  void set_error_code_sensor(sensor::Sensor *error_code_sensor) { error_code_sensor_ = error_code_sensor; }
  void set_tracker_operation_sensor(sensor::Sensor *tracker_operation_sensor) {
    tracker_operation_sensor_ = tracker_operation_sensor;
  }
  void set_charger_text_sensor(text_sensor::TextSensor *charger_text_sensor) {
    charger_text_sensor_ = charger_text_sensor;
  }
  void set_error_text_sensor(text_sensor::TextSensor *error_text_sensor) { error_text_sensor_ = error_text_sensor; }
  void set_tracker_text_sensor(text_sensor::TextSensor *tracker_text_sensor) {
    tracker_text_sensor_ = tracker_text_sensor;
  }
  void set_fw_version_sensor(text_sensor::TextSensor *fw_version_sensor) { fw_version_sensor_ = fw_version_sensor; }
  void set_pid_sensor(text_sensor::TextSensor *pid_sensor) { pid_sensor_ = pid_sensor; }
  void set_number_of_charge_cycles_sensor(sensor::Sensor *number_of_charge_cycles_sensor) {
    number_of_charge_cycles_sensor_ = number_of_charge_cycles_sensor;
  }
  void set_number_of_full_discharges_sensor(sensor::Sensor *number_of_full_discharges_sensor) {
    number_of_full_discharges_sensor_ = number_of_full_discharges_sensor;
  }
  void set_last_full_charge_sensor(sensor::Sensor *last_full_charge_sensor) {
    last_full_charge_sensor_ = last_full_charge_sensor;
  }  // H9 sec Number of seconds since last full charge
  void set_depth_deepest_dis_sensor(sensor::Sensor *depth_deepest_dis_sensor) {
    depth_deepest_dis_sensor_ = depth_deepest_dis_sensor;
  }  // H1 mAh Depth of the deepest discharge
  void set_depth_of_the_last_discharge_sensor(sensor::Sensor *depth_of_the_last_discharge_sensor) {
    depth_of_the_last_discharge_sensor_ = depth_of_the_last_discharge_sensor;
  }  // H2 mAh Depth of the last discharge
  void set_amount_of_discharged_energy_sensor(sensor::Sensor *amount_of_discharged_energy_sensor) {
    amount_of_discharged_energy_sensor_ = amount_of_discharged_energy_sensor;
  }  // H17 0.01 kWh Amount of discharged energy

  void set_time_to_go_sensor(sensor::Sensor *time_to_go_sensor) { time_to_go_sensor_ = time_to_go_sensor; }
  void set_consumed_amp_hours_sensor(sensor::Sensor *consumed_amp_hours_sensor) {
    consumed_amp_hours_sensor_ = consumed_amp_hours_sensor;
  }
  void set_instantaneous_power_sensor(sensor::Sensor *instantaneous_power_sensor) {
    instantaneous_power_sensor_ = instantaneous_power_sensor;
  }
  void set_state_of_charge_sensor(sensor::Sensor *state_of_charge_sensor) {
    state_of_charge_sensor_ = state_of_charge_sensor;
  }

  void set_min_battery_voltage_sensor(sensor::Sensor *min_battery_voltage_sensor) {
    min_battery_voltage_sensor_ = min_battery_voltage_sensor;
  }
  void set_max_battery_voltage_sensor(sensor::Sensor *max_battery_voltage_sensor) {
    max_battery_voltage_sensor_ = max_battery_voltage_sensor;
  }
  void set_amount_of_charged_sensor(sensor::Sensor *amount_of_charged_sensor) {
    amount_of_charged_sensor_ = amount_of_charged_sensor;
  }

  void set_bmv_alarm_sensor(text_sensor::TextSensor *bmv_alarm_sensor) { bmv_alarm_sensor_ = bmv_alarm_sensor; }
  void set_bmv_alarm_reason_sensor(text_sensor::TextSensor *bmv_alarm_reason_sensor) {
    bmv_alarm_reason_sensor_ = bmv_alarm_reason_sensor;
  }
  void set_bmv_sensor(text_sensor::TextSensor *bmv_sensor) { bmv_sensor_ = bmv_sensor; }

  // SmartShunt end
  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  // One entry per VE.Direct label this component understands. The numeric fields come first and
  // double as an index into cache_ (publish-on-change), F_NUM_SENSORS marks the end of that range.
  enum Field : uint8_t {
    F_MAX_POWER_YESTERDAY = 0,
    F_MAX_POWER_TODAY,
    F_YIELD_TOTAL,
    F_YIELD_YESTERDAY,
    F_YIELD_TODAY,
    F_PANEL_VOLTAGE,
    F_PANEL_POWER,
    F_BATTERY_VOLTAGE,
    F_BATTERY_CURRENT,
    F_LOAD_CURRENT,
    F_DAY_NUMBER,
    F_CHARGER_STATUS,
    F_ERROR_CODE,
    F_TRACKER_OPERATION,
    F_INSTANTANEOUS_POWER,
    F_TIME_TO_GO,
    F_STATE_OF_CHARGE,
    F_CONSUMED_AMP_HOURS,
    F_MIN_BATTERY_VOLTAGE,
    F_MAX_BATTERY_VOLTAGE,
    F_AMOUNT_OF_CHARGED,
    F_LAST_FULL_CHARGE,
    F_DEPTH_DEEPEST_DIS,
    F_DEPTH_LAST_DIS,
    F_AMOUNT_OF_DISCHARGED,
    F_NUMBER_OF_FULL_DIS,
    F_NUMBER_OF_CHARGE_CYCLES,
    F_NUM_SENSORS,  // marker, not a field
    F_BMV = F_NUM_SENSORS,
    F_ALARM,
    F_ALARM_REASON,
    F_FW,
    F_PID,
    F_UNKNOWN,
  };

  // VE.Direct text protocol buffer sizes: the spec recommends 9 bytes for a label and 33 for a value,
  // a little margin is kept here. A longer field is dropped instead of being truncated.
  static constexpr size_t LABEL_MAX = 15;
  static constexpr size_t VALUE_MAX = 47;
  // The fields of a block are kept here until its checksum has been verified (as "label\0value\0").
  static constexpr size_t PENDING_BYTES = 768;

  struct PubCache {
    float value;
    uint32_t ms;
    bool valid;
  };

  void process_byte_(uint8_t c);
  void reset_parser_();
  void store_field_();
  void finish_block_(bool checksum_ok);
  void handle_value_(const char *label, const char *value);
  sensor::Sensor *sensor_for_(Field field) const;
  static Field lookup_field_(const char *label);
  // Publish only if the value changed or the last publish is older than REFRESH_INTERVAL_MS.
  void publish_(Field field, sensor::Sensor *sens, float value);
  void publish_text_(text_sensor::TextSensor *sens, const char *value);

  sensor::Sensor *max_power_yesterday_sensor_{nullptr};
  sensor::Sensor *max_power_today_sensor_{nullptr};
  sensor::Sensor *yield_total_sensor_{nullptr};
  sensor::Sensor *yield_yesterday_sensor_{nullptr};
  sensor::Sensor *yield_today_sensor_{nullptr};
  sensor::Sensor *panel_voltage_sensor_{nullptr};
  sensor::Sensor *panel_power_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *battery_current_sensor_{nullptr};
  sensor::Sensor *load_current_sensor_{nullptr};
  sensor::Sensor *day_number_sensor_{nullptr};
  sensor::Sensor *charger_status_sensor_{nullptr};
  sensor::Sensor *error_code_sensor_{nullptr};
  sensor::Sensor *tracker_operation_sensor_{nullptr};
  // SS
  sensor::Sensor *instantaneous_power_sensor_{nullptr};
  sensor::Sensor *time_to_go_sensor_{nullptr};
  sensor::Sensor *state_of_charge_sensor_{nullptr};
  sensor::Sensor *consumed_amp_hours_sensor_{nullptr};

  sensor::Sensor *min_battery_voltage_sensor_{nullptr};
  sensor::Sensor *max_battery_voltage_sensor_{nullptr};
  sensor::Sensor *amount_of_charged_sensor_{nullptr};
  sensor::Sensor *last_full_charge_sensor_{nullptr};
  sensor::Sensor *depth_deepest_dis_sensor_{nullptr};
  sensor::Sensor *depth_of_the_last_discharge_sensor_{nullptr};
  sensor::Sensor *amount_of_discharged_energy_sensor_{nullptr};
  sensor::Sensor *number_of_charge_cycles_sensor_{nullptr};
  sensor::Sensor *number_of_full_discharges_sensor_{nullptr};
  text_sensor::TextSensor *bmv_alarm_sensor_{nullptr};
  text_sensor::TextSensor *bmv_alarm_reason_sensor_{nullptr};
  text_sensor::TextSensor *bmv_sensor_{nullptr};
  // SS
  text_sensor::TextSensor *charger_text_sensor_{nullptr};
  text_sensor::TextSensor *error_text_sensor_{nullptr};
  text_sensor::TextSensor *tracker_text_sensor_{nullptr};
  text_sensor::TextSensor *fw_version_sensor_{nullptr};
  text_sensor::TextSensor *pid_sensor_{nullptr};

  // 0: between fields, 1: reading a label, 2: reading a value, 3: next byte is the checksum
  int state_{0};
  bool in_hex_{false};  // inside an asynchronous HEX frame (":...\n"), ignored
  uint8_t sum_{0};      // running modulo 256 sum of the bytes of the current block
  char pending_[PENDING_BYTES]{};
  size_t pending_len_{0};
  bool pending_overflow_{false};
  uint32_t checksum_errors_{0};
  char label_[LABEL_MAX + 1]{};
  char value_[VALUE_MAX + 1]{};
  size_t label_len_{0};
  size_t value_len_{0};
  bool overflow_{false};
  uint32_t last_transmission_{0};
  PubCache cache_[F_NUM_SENSORS]{};
};

}  // namespace victron_smart_shunt
}  // namespace esphome
