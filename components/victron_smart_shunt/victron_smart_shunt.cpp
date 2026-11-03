#include "victron_smart_shunt.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace victron_smart_shunt {

static const char *const TAG = "victron_smart_shunt";

void VictronSmartShuntComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "VictronSmartShunt:");
  // LOG_SENSOR("  ", "Max Power Yesterday", max_power_yesterday_sensor_);
  // LOG_SENSOR("  ", "Max Power Today", max_power_today_sensor_);
  // LOG_SENSOR("  ", "Yield Total", yield_total_sensor_);
  // LOG_SENSOR("  ", "Yield Yesterday", yield_yesterday_sensor_);
  // LOG_SENSOR("  ", "Yield Today", yield_today_sensor_);
  // LOG_SENSOR("  ", "Panel Voltage", panel_voltage_sensor_);
  // LOG_SENSOR("  ", "Panel Power", panel_power_sensor_);
  // LOG_SENSOR("  ", "Battery Voltage", battery_voltage_sensor_);
  // LOG_SENSOR("  ", "Battery Current", battery_current_sensor_);
  // LOG_SENSOR("  ", "Load Current", load_current_sensor_);
  // LOG_SENSOR("  ", "Day Number", day_number_sensor_);
  // LOG_SENSOR("  ", "Charger Status", charger_status_sensor_);
  // LOG_SENSOR("  ", "Error Code", error_code_sensor_);
  // LOG_TEXT_SENSOR("  ", "Charger Text", charger_text_sensor_);
  // LOG_TEXT_SENSOR("  ", "Error Text", error_text_sensor_);
  // LOG_TEXT_SENSOR("  ", "Tracker Operation", tracker_operation_sensor_);
  // LOG_TEXT_SENSOR("  ", "FW Version", fw_version_sensor_);
  // LOG_TEXT_SENSOR("  ", "PID", pid_sensor_);

  // LOG_SENSOR("  ", "Instantaneous power", instantaneous_power_sensor_);
  // LOG_SENSOR("  ", "Time to go", time_to_go_sensor_);
  // LOG_SENSOR("  ", "Consumed amp hours", consumed_amp_hours_sensor_);
  // LOG_SENSOR("  ", "State of Charge", state_of_charge_sensor_);

  // LOG_TEXT_SENSOR("  ", "BMV-Alarm", bmv_alarm_sensor_);
  // LOG_TEXT_SENSOR("  ", "BMV-Alarm Reason", bmv_alarm_reason_sensor_);
  // LOG_TEXT_SENSOR("  ", "BMV-PID", bmv_sensor_);

  // LOG_SENSOR("  ", "Minimum main (battery) voltage", min_battery_voltage_sensor_);
  // LOG_SENSOR("  ", "Maximum main (battery) voltage", max_battery_voltage_sensor_);
  // LOG_SENSOR("  ", "Amount of charged energy", amount_of_charged_sensor_);

  // LOG_SENSOR("  ", "Number of charge cycles", number_of_charge_cycles_sensor_);
  // LOG_SENSOR("  ", "Number of full discharges", number_of_full_discharges_sensor_);

  // LOG_SENSOR("  ", "Number of seconds since last full charge", last_full_charge_sensor_);
  // LOG_SENSOR("  ", "Depth of the deepest discharge", depth_deepest_dis_sensor_);
  // LOG_SENSOR("  ", "Depth of the last discharge", depth_of_the_last_discharge_sensor_);
  // LOG_SENSOR("  ", "Amount of discharged energy", amount_of_discharged_energy_sensor_);

  // check_uart_settings(19200);
}

// A block is sent once per second. The slow numbers and the text sensors that did not change are not
// re-published more often than this (keeps HA's last_updated fresh and gives user filters a sample).
static const uint32_t REFRESH_INTERVAL_MS = 60000;

void VictronSmartShuntComponent::loop() {
  const uint32_t now = millis();
  if ((state_ > 0) && (now - last_transmission_ >= 200)) {
    // last transmission too long ago. Reset RX index.
    ESP_LOGW(TAG, "Last transmission too long ago.");
    this->reset_parser_();
  }

  size_t avail = this->available();
  if (avail == 0)
    return;

  last_transmission_ = now;
  // Read in chunks instead of one byte (= one driver call) at a time.
  uint8_t buf[64];
  while (avail > 0) {
    const size_t n = avail < sizeof(buf) ? avail : sizeof(buf);
    if (!this->read_array(buf, n))
      break;
    for (size_t i = 0; i < n; i++)
      this->process_byte_(buf[i]);
    avail = this->available();
  }
}

void VictronSmartShuntComponent::reset_parser_() {
  state_ = 0;
  in_hex_ = false;
  sum_ = 0;
  pending_len_ = 0;
  pending_overflow_ = false;
}

void VictronSmartShuntComponent::process_byte_(uint8_t c) {
  if (state_ == 3) {
    // The checksum byte can have any value (even ':', '\r', '\n' or '\t'). The modulo 256 sum of all the
    // bytes of the block, checksum included, is 0 when there was no transmission error.
    sum_ += c;
    this->finish_block_(sum_ == 0);
    return;
  }
  // Asynchronous HEX frames (":A...\n") can be inserted anywhere in a text block. They are not part of the
  // text block: skipped, and not counted in the checksum.
  if (in_hex_) {
    if (c == '\n')
      in_hex_ = false;
    return;
  }
  if (c == ':') {
    in_hex_ = true;
    return;
  }
  sum_ += c;

  if (state_ == 0) {
    if ((c == '\r') || (c == '\n'))
      return;
    label_len_ = 0;
    value_len_ = 0;
    overflow_ = false;
    state_ = 1;
  }
  if (state_ == 1) {
    if (c == '\t') {
      label_[label_len_] = '\0';
      state_ = (strcmp(label_, "Checksum") == 0) ? 3 : 2;
    } else if (label_len_ < LABEL_MAX) {
      label_[label_len_++] = (char) c;
    } else {
      overflow_ = true;
    }
    return;
  }
  // state_ == 2
  if ((c == '\r') || (c == '\n')) {
    value_[value_len_] = '\0';
    this->store_field_();
    state_ = 0;
  } else if (value_len_ < VALUE_MAX) {
    value_[value_len_++] = (char) c;
  } else {
    overflow_ = true;
  }
}

// Keep the field until the checksum of the block is known.
void VictronSmartShuntComponent::store_field_() {
  if (overflow_ || strlen(label_) != label_len_ || strlen(value_) != value_len_)
    return;  // too long or containing a NUL: dropped, like before
  if (pending_len_ + label_len_ + value_len_ + 2 > PENDING_BYTES) {
    pending_overflow_ = true;
    return;
  }
  memcpy(pending_ + pending_len_, label_, label_len_ + 1);
  pending_len_ += label_len_ + 1;
  memcpy(pending_ + pending_len_, value_, value_len_ + 1);
  pending_len_ += value_len_ + 1;
}

void VictronSmartShuntComponent::finish_block_(bool checksum_ok) {
  if (checksum_ok && !pending_overflow_) {
    const char *p = pending_;
    const char *const end = pending_ + pending_len_;
    while (p < end) {
      const char *label = p;
      p += strlen(label) + 1;
      const char *value = p;
      p += strlen(value) + 1;
      this->handle_value_(label, value);
    }
  } else {
    checksum_errors_++;
    ESP_LOGW(TAG, "Block dropped (%s), %u dropped since boot", pending_overflow_ ? "too large" : "bad checksum",
             (unsigned) checksum_errors_);
  }
  this->reset_parser_();
}

void VictronSmartShuntComponent::publish_(Field field, sensor::Sensor *sens, float value) {
  if (sens == nullptr)
    return;
  // The fast values (voltage, current, power, SOC...) are published at every block, even unchanged: they
  // can feed a PID or any logic that expects a regular update. Only the slow fields are de-duplicated.
  bool fast = false;
  switch (field) {
    case F_BATTERY_VOLTAGE:
    case F_BATTERY_CURRENT:
    case F_INSTANTANEOUS_POWER:
    case F_STATE_OF_CHARGE:
    case F_CONSUMED_AMP_HOURS:
    case F_TIME_TO_GO:
    case F_PANEL_VOLTAGE:
    case F_PANEL_POWER:
    case F_LOAD_CURRENT:
      fast = true;
      break;
    default:
      break;
  }
  PubCache &c = cache_[field];
  const uint32_t now = millis();
  const bool same = (c.value == value) || (std::isnan(c.value) && std::isnan(value));
  if (!fast && c.valid && same && (now - c.ms) < REFRESH_INTERVAL_MS)
    return;
  c.valid = true;
  c.value = value;
  c.ms = now;
  sens->publish_state(value);
}

void VictronSmartShuntComponent::publish_text_(text_sensor::TextSensor *sens, const char *value) {
  if (sens == nullptr)
    return;
  if (sens->has_state() && sens->state == value)
    return;
  sens->publish_state(value);
}

static const char *charger_status_text(int value) {
  switch (value) {
    case 0:
      return "Off";
    case 2:
      return "Fault";
    case 3:
      return "Bulk";
    case 4:
      return "Absorption";
    case 5:
      return "Float";
    case 7:
      return "Equalize (manual)";
    case 245:
      return "Starting-up";
    case 247:
      return "Auto equalize / Recondition";
    case 252:
      return "External control";
    default:
      return "Unknown";
  }
}

static const char *error_code_text(int value) {
  switch (value) {
    case 0:
      return "No error";
    case 1:
      return "Low Voltage";
    case 2:
      return "Battery voltage too high";
    case 4:
      return "Low SOC";
    case 8:
      return "Low Starte Voltage";
    case 16:
      return "High Starter Voltage";
    case 17:
      return "Charger temperature too high";
    case 18:
      return "Charger over current";
    case 19:
      return "Charger current reversed";
    case 20:
      return "Bulk time limit exceeded";
    case 21:
      return "Current sensor issue";
    case 26:
      return "Terminals overheated";
    case 28:
      return "Converter issue";
    case 32:
      return "Low Temperature";
    case 33:
      return "Input voltage too high (solar panel)";
    case 34:
      return "Input current too high (solar panel)";
    case 38:
      return "Input shutdown (excessive battery voltage)";
    case 39:
      return "Input shutdown (due to current flow during off mode)";
    case 64:
      return "High Temperature";
    case 65:
      return "Lost communication with one of devices";
    case 66:
      return "Synchronised charging device configuration issue";
    case 67:
      return "BMS connection lost";
    case 68:
      return "Network misconfigured";
    case 116:
      return "Factory calibration data lost";
    case 117:
      return "Invalid/incompatible firmware";
    case 119:
      return "User settings invalid";
    case 128:
      return "Mid Voltage";
    default:
      return "Unknown";
  }
}

static const char *tracker_op_text(int value) {
  switch (value) {
    case 0:
      return "Off";
    case 1:
      return "Limited";
    case 2:
      return "Active";
    default:
      return "Unknown";
  }
}

// AR (alarm reason) is a bit field, the values of the active alarms are added and sent in decimal
// (e.g. 5 = low voltage + low SOC).
static const size_t ALARM_TEXT_MAX = 192;
static void alarm_reason_text(int value, char *out, size_t len) {
  static const struct {
    int bit;
    const char *text;
  } ALARMS[] = {
      {1, "Low Voltage"},
      {2, "Battery voltage too high"},
      {4, "Low SOC"},
      {8, "Low Starter Voltage"},
      {16, "High Starter Voltage"},
      {32, "Low Temperature"},
      {64, "High Temperature"},
      {128, "Mid Voltage"},
  };
  out[0] = '\0';
  if (value == 0) {
    snprintf(out, len, "No error");
    return;
  }
  size_t used = 0;
  for (const auto &a : ALARMS) {
    if ((value & a.bit) == 0)
      continue;
    const int w = snprintf(out + used, len - used, "%s%s", used ? ", " : "", a.text);
    if (w < 0 || (size_t) w >= len - used)
      return;
    used += (size_t) w;
  }
  if ((value & ~0xFF) != 0 || value < 0) {
    snprintf(out + used, len - used, "%sUnknown", used ? ", " : "");
  }
}

static const char *pid_text(int value) {
  switch (value) {
    case 0x203:
      return "BMV-700";
    case 0x204:
      return "BMV-702";
    case 0x205:
      return "BMV-700H";
    case 0xA389:
      return "SmartShunt 500A/50mV";
    case 0xA38A:
      return "SmartShunt 1000A/50mV";
    case 0xA38B:
      return "SmartShunt 2000A/50mV";
    case 0xA381:
      return "BMV-712 Smart";
    case 0xA382:
      return "BMV-710H Smart";
    case 0xA383:
      return "BMV-712 Smart Rev2";
    case 0xA04C:
      return "BlueSolar MPPT 75/10";
    case 0x300:
      return "BlueSolar MPPT 70/15";
    case 0xA042:
      return "BlueSolar MPPT 75/15";
    case 0xA043:
      return "BlueSolar MPPT 100/15";
    case 0xA044:
      return "BlueSolar MPPT 100/30 rev1";
    case 0xA04A:
      return "BlueSolar MPPT 100/30 rev2";
    case 0xA041:
      return "BlueSolar MPPT 150/35 rev1";
    case 0xA04B:
      return "BlueSolar MPPT 150/35 rev2";
    case 0xA04D:
      return "BlueSolar MPPT 150/45";
    case 0xA040:
      return "BlueSolar MPPT 75/50";
    case 0xA045:
      return "BlueSolar MPPT 100/50 rev1";
    case 0xA049:
      return "BlueSolar MPPT 100/50 rev2";
    case 0xA04E:
      return "BlueSolar MPPT 150/60";
    case 0xA046:
      return "BlueSolar MPPT 150/70";
    case 0xA04F:
      return "BlueSolar MPPT 150/85";
    case 0xA047:
      return "BlueSolar MPPT 150/100";
    case 0xA050:
      return "SmartSolar MPPT 250/100";
    case 0xA051:
      return "SmartSolar MPPT 150/100";
    case 0xA052:
      return "SmartSolar MPPT 150/85";
    case 0xA053:
      return "SmartSolar MPPT 75/15";
    case 0xA054:
      return "SmartSolar MPPT 75/10";
    case 0xA055:
      return "SmartSolar MPPT 100/15";
    case 0xA056:
      return "SmartSolar MPPT 100/30";
    case 0xA057:
      return "SmartSolar MPPT 100/50";
    case 0xA058:
      return "SmartSolar MPPT 150/35";
    case 0xA059:
      return "SmartSolar MPPT 150/100 rev2";
    case 0xA05A:
      return "SmartSolar MPPT 150/85 rev2";
    case 0xA05B:
      return "SmartSolar MPPT 250/70";
    case 0xA05C:
      return "SmartSolar MPPT 250/85";
    case 0xA05D:
      return "SmartSolar MPPT 250/60";
    case 0xA05E:
      return "SmartSolar MPPT 250/45";
    case 0xA05F:
      return "SmartSolar MPPT 100/20";
    case 0xA060:
      return "SmartSolar MPPT 100/20 48V";
    case 0xA061:
      return "SmartSolar MPPT 150/45";
    case 0xA062:
      return "SmartSolar MPPT 150/60";
    case 0xA063:
      return "SmartSolar MPPT 150/70";
    case 0xA064:
      return "SmartSolar MPPT 250/85 rev2";
    case 0xA065:
      return "SmartSolar MPPT 250/100 rev2";
    case 0xA201:
      return "Phoenix Inverter 12V 250VA 230V";
    case 0xA202:
      return "Phoenix Inverter 24V 250VA 230V";
    case 0xA204:
      return "Phoenix Inverter 48V 250VA 230V";
    case 0xA211:
      return "Phoenix Inverter 12V 375VA 230V";
    case 0xA212:
      return "Phoenix Inverter 24V 375VA 230V";
    case 0xA214:
      return "Phoenix Inverter 48V 375VA 230V";
    case 0xA221:
      return "Phoenix Inverter 12V 500VA 230V";
    case 0xA222:
      return "Phoenix Inverter 24V 500VA 230V";
    case 0xA224:
      return "Phoenix Inverter 48V 500VA 230V";
    case 0xA231:
      return "Phoenix Inverter 12V 250VA 230V";
    case 0xA232:
      return "Phoenix Inverter 24V 250VA 230V";
    case 0xA234:
      return "Phoenix Inverter 48V 250VA 230V";
    case 0xA239:
      return "Phoenix Inverter 12V 250VA 120V";
    case 0xA23A:
      return "Phoenix Inverter 24V 250VA 120V";
    case 0xA23C:
      return "Phoenix Inverter 48V 250VA 120V";
    case 0xA241:
      return "Phoenix Inverter 12V 375VA 230V";
    case 0xA242:
      return "Phoenix Inverter 24V 375VA 230V";
    case 0xA244:
      return "Phoenix Inverter 48V 375VA 230V";
    case 0xA249:
      return "Phoenix Inverter 12V 375VA 120V";
    case 0xA24A:
      return "Phoenix Inverter 24V 375VA 120V";
    case 0xA24C:
      return "Phoenix Inverter 48V 375VA 120V";
    case 0xA251:
      return "Phoenix Inverter 12V 500VA 230V";
    case 0xA252:
      return "Phoenix Inverter 24V 500VA 230V";
    case 0xA254:
      return "Phoenix Inverter 48V 500VA 230V";
    case 0xA259:
      return "Phoenix Inverter 12V 500VA 120V";
    case 0xA25A:
      return "Phoenix Inverter 24V 500VA 120V";
    case 0xA25C:
      return "Phoenix Inverter 48V 500VA 120V";
    case 0xA261:
      return "Phoenix Inverter 12V 800VA 230V";
    case 0xA262:
      return "Phoenix Inverter 24V 800VA 230V";
    case 0xA264:
      return "Phoenix Inverter 48V 800VA 230V";
    case 0xA269:
      return "Phoenix Inverter 12V 800VA 120V";
    case 0xA26A:
      return "Phoenix Inverter 24V 800VA 120V";
    case 0xA26C:
      return "Phoenix Inverter 48V 800VA 120V";
    case 0xA271:
      return "Phoenix Inverter 12V 1200VA 230V";
    case 0xA272:
      return "Phoenix Inverter 24V 1200VA 230V";
    case 0xA274:
      return "Phoenix Inverter 48V 1200VA 230V";
    case 0xA279:
      return "Phoenix Inverter 12V 1200VA 120V";
    case 0xA27A:
      return "Phoenix Inverter 24V 1200VA 120V";
    case 0xA27C:
      return "Phoenix Inverter 48V 1200VA 120V";
    default:
      return "Unknown";
  }
}

VictronSmartShuntComponent::Field VictronSmartShuntComponent::lookup_field_(const char *label) {
  struct Entry {
    const char *label;
    Field field;
  };
  // Checked linearly, the labels that change every block come first.
  static const Entry TABLE[] = {
      {"V", F_BATTERY_VOLTAGE},
      {"I", F_BATTERY_CURRENT},
      {"P", F_INSTANTANEOUS_POWER},
      {"CE", F_CONSUMED_AMP_HOURS},
      {"SOC", F_STATE_OF_CHARGE},
      {"TTG", F_TIME_TO_GO},
      {"Alarm", F_ALARM},
      {"AR", F_ALARM_REASON},
      {"H1", F_DEPTH_DEEPEST_DIS},
      {"H2", F_DEPTH_LAST_DIS},
      {"H4", F_NUMBER_OF_CHARGE_CYCLES},
      {"H5", F_NUMBER_OF_FULL_DIS},
      {"H7", F_MIN_BATTERY_VOLTAGE},
      {"H8", F_MAX_BATTERY_VOLTAGE},
      {"H9", F_LAST_FULL_CHARGE},
      {"H17", F_AMOUNT_OF_DISCHARGED},
      {"H18", F_AMOUNT_OF_CHARGED},
      {"PID", F_PID},
      {"FW", F_FW},
      {"BMV", F_BMV},
      // MPPT labels (kept from the shared VE.Direct code)
      {"VPV", F_PANEL_VOLTAGE},
      {"PPV", F_PANEL_POWER},
      {"IL", F_LOAD_CURRENT},
      {"CS", F_CHARGER_STATUS},
      {"ERR", F_ERROR_CODE},
      {"MPPT", F_TRACKER_OPERATION},
      {"HSDS", F_DAY_NUMBER},
      {"H19", F_YIELD_TOTAL},
      {"H20", F_YIELD_TODAY},
      {"H21", F_MAX_POWER_TODAY},
      {"H22", F_YIELD_YESTERDAY},
      {"H23", F_MAX_POWER_YESTERDAY},
  };
  for (const Entry &e : TABLE) {
    if (strcmp(label, e.label) == 0)
      return e.field;
  }
  return F_UNKNOWN;
}

// 1: integer parsed, 0: "---" (value not available: shunt not synchronised, no sensor...), -1: not a number
static int parse_int(const char *s, int &out) {
  if (strcmp(s, "---") == 0)
    return 0;
  char *end = nullptr;
  const long v = strtol(s, &end, 10);
  if (end == s || *end != '\0')
    return -1;
  out = (int) v;
  return 1;
}

sensor::Sensor *VictronSmartShuntComponent::sensor_for_(Field field) const {
  switch (field) {
    case F_MAX_POWER_YESTERDAY:
      return max_power_yesterday_sensor_;
    case F_MAX_POWER_TODAY:
      return max_power_today_sensor_;
    case F_YIELD_TOTAL:
      return yield_total_sensor_;
    case F_YIELD_YESTERDAY:
      return yield_yesterday_sensor_;
    case F_YIELD_TODAY:
      return yield_today_sensor_;
    case F_PANEL_VOLTAGE:
      return panel_voltage_sensor_;
    case F_PANEL_POWER:
      return panel_power_sensor_;
    case F_BATTERY_VOLTAGE:
      return battery_voltage_sensor_;
    case F_BATTERY_CURRENT:
      return battery_current_sensor_;
    case F_LOAD_CURRENT:
      return load_current_sensor_;
    case F_DAY_NUMBER:
      return day_number_sensor_;
    case F_CHARGER_STATUS:
      return charger_status_sensor_;
    case F_ERROR_CODE:
      return error_code_sensor_;
    case F_TRACKER_OPERATION:
      return tracker_operation_sensor_;
    case F_INSTANTANEOUS_POWER:
      return instantaneous_power_sensor_;
    case F_TIME_TO_GO:
      return time_to_go_sensor_;
    case F_STATE_OF_CHARGE:
      return state_of_charge_sensor_;
    case F_CONSUMED_AMP_HOURS:
      return consumed_amp_hours_sensor_;
    case F_MIN_BATTERY_VOLTAGE:
      return min_battery_voltage_sensor_;
    case F_MAX_BATTERY_VOLTAGE:
      return max_battery_voltage_sensor_;
    case F_AMOUNT_OF_CHARGED:
      return amount_of_charged_sensor_;
    case F_LAST_FULL_CHARGE:
      return last_full_charge_sensor_;
    case F_DEPTH_DEEPEST_DIS:
      return depth_deepest_dis_sensor_;
    case F_DEPTH_LAST_DIS:
      return depth_of_the_last_discharge_sensor_;
    case F_AMOUNT_OF_DISCHARGED:
      return amount_of_discharged_energy_sensor_;
    case F_NUMBER_OF_FULL_DIS:
      return number_of_full_discharges_sensor_;
    case F_NUMBER_OF_CHARGE_CYCLES:
      return number_of_charge_cycles_sensor_;
    default:
      return nullptr;
  }
}

void VictronSmartShuntComponent::handle_value_(const char *label, const char *value) {
  const Field field = lookup_field_(label);

  int n = 0;
  if (field < F_NUM_SENSORS || field == F_ALARM_REASON) {
    // numeric field
    const int parsed = parse_int(value, n);
    if (parsed == 0) {
      // "---": the device has no value (CE, SOC and TTG while not synchronised...). Publish NaN (state
      // "unknown" in HA) instead of a fake 0.
      publish_(field, sensor_for_(field), NAN);
      return;
    }
    if (parsed < 0) {
      ESP_LOGD(TAG, "   ----> LABEL : '%s' has a non numeric value : '%s'", label, value);
      return;
    }
  }

  switch (field) {
    // ---- SmartShunt / BMV
    case F_NUMBER_OF_FULL_DIS:
      publish_(field, number_of_full_discharges_sensor_, n);
      break;
    case F_NUMBER_OF_CHARGE_CYCLES:
      publish_(field, number_of_charge_cycles_sensor_, n);
      break;
    case F_LAST_FULL_CHARGE:
      // sec -> min
      publish_(field, last_full_charge_sensor_, (float) n / 60.0);
      break;
    case F_DEPTH_DEEPEST_DIS:
      // mAh -> Ah
      publish_(field, depth_deepest_dis_sensor_, n / 1000.0);
      break;
    case F_DEPTH_LAST_DIS:
      // mAh -> Ah
      publish_(field, depth_of_the_last_discharge_sensor_, n / 1000.0);
      break;
    case F_AMOUNT_OF_DISCHARGED:
      // 0.01 kWh -> Wh
      publish_(field, amount_of_discharged_energy_sensor_, n * 10.00);
      break;
    case F_AMOUNT_OF_CHARGED:
      // 0.01 kWh -> Wh
      publish_(field, amount_of_charged_sensor_, n * 10.00);
      break;
    case F_BMV:
      publish_text_(bmv_sensor_, value);
      break;
    case F_ALARM:
      publish_text_(bmv_alarm_sensor_, value);
      break;
    case F_MIN_BATTERY_VOLTAGE:
      // mV -> V
      publish_(field, min_battery_voltage_sensor_, n / 1000.00);
      break;
    case F_MAX_BATTERY_VOLTAGE:
      // mV -> V
      publish_(field, max_battery_voltage_sensor_, n / 1000.00);
      break;
    case F_TIME_TO_GO:
      publish_(field, time_to_go_sensor_, n);
      break;
    case F_STATE_OF_CHARGE:
      // permille -> %
      publish_(field, state_of_charge_sensor_, n * 0.10);
      break;
    case F_CONSUMED_AMP_HOURS:
      // mAh -> Ah
      publish_(field, consumed_amp_hours_sensor_, n / 1000.00);
      break;
    case F_INSTANTANEOUS_POWER:
      publish_(field, instantaneous_power_sensor_, n);
      break;
    case F_ALARM_REASON:
      {
        char text[ALARM_TEXT_MAX];
        alarm_reason_text(n, text, sizeof(text));
        publish_text_(bmv_alarm_reason_sensor_, text);
      }
      break;

    // ---- shared with MPPT / SmartShunt
    case F_BATTERY_VOLTAGE:
      publish_(field, battery_voltage_sensor_, n / 1000.0);
      break;
    case F_BATTERY_CURRENT:
      publish_(field, battery_current_sensor_, n / 1000.0);
      break;

    // ---- MPPT
    case F_MAX_POWER_YESTERDAY:
      publish_(field, max_power_yesterday_sensor_, n);
      break;
    case F_MAX_POWER_TODAY:
      publish_(field, max_power_today_sensor_, n);
      break;
    case F_YIELD_TOTAL:
      publish_(field, yield_total_sensor_, n * 10);
      break;
    case F_YIELD_YESTERDAY:
      publish_(field, yield_yesterday_sensor_, n * 10);
      break;
    case F_YIELD_TODAY:
      publish_(field, yield_today_sensor_, n * 10);
      break;
    case F_PANEL_VOLTAGE:
      publish_(field, panel_voltage_sensor_, n / 1000.0);
      break;
    case F_PANEL_POWER:
      publish_(field, panel_power_sensor_, n);
      break;
    case F_LOAD_CURRENT:
      publish_(field, load_current_sensor_, n / 1000.0);
      break;
    case F_DAY_NUMBER:
      publish_(field, day_number_sensor_, n);
      break;
    case F_CHARGER_STATUS:
      publish_(field, charger_status_sensor_, n);
      publish_text_(charger_text_sensor_, charger_status_text(n));
      break;
    case F_ERROR_CODE:
      publish_(field, error_code_sensor_, n);
      publish_text_(error_text_sensor_, error_code_text(n));
      break;
    case F_TRACKER_OPERATION:
      publish_(field, tracker_operation_sensor_, n);
      publish_text_(tracker_text_sensor_, tracker_op_text(n));
      break;

    // ---- identification, published once
    case F_FW:
      if ((fw_version_sensor_ != nullptr) && !fw_version_sensor_->has_state()) {
        std::string fw(value);
        if (fw.size() >= 2)
          fw.insert(fw.size() - 2, ".");
        fw_version_sensor_->publish_state(fw);
      }
      break;
    case F_PID:
      if ((pid_sensor_ != nullptr) && !pid_sensor_->has_state())
        pid_sensor_->publish_state(pid_text(strtol(value, nullptr, 0)));
      break;

    default:
      ESP_LOGD(TAG, "   ----> unhalted LABEL : '%s'  with value : '%s'", label, value);
      break;
  }
}

}  // namespace victron_smart_shunt
}  // namespace esphome
