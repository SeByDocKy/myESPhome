#include "victron.h"
#include "esphome/core/log.h"
#include <algorithm>  // std::min
#include <functional>
#include <unordered_map>
#include "esphome/core/helpers.h"

namespace esphome::victron {

static constexpr char TAG[] = "victron";

static constexpr uint8_t OFF_REASONS_SIZE = 16;
static constexpr const char *OFF_REASONS[OFF_REASONS_SIZE] = {
    "No input power",                       // 0000 0000 0000 0001
    "Switched off (power switch)",          // 0000 0000 0000 0010
    "Switched off (device mode register)",  // 0000 0000 0000 0100
    "Remote input",                         // 0000 0000 0000 1000
    "Protection active",                    // 0000 0000 0001 0000
    "Paygo",                                // 0000 0000 0010 0000
    "BMS",                                  // 0000 0000 0100 0000
    "Engine shutdown detection",            // 0000 0000 1000 0000
    "Analysing input voltage",              // 0000 0001 0000 0000
    "Unknown: Bit 10",                      // 0000 0010 0000 0000
    "Unknown: Bit 11",                      // 0000 0100 0000 0000
    "Unknown: Bit 12",                      // 0000 1000 0000 0000
    "Unknown: Bit 13",                      // 0001 0000 0000 0000
    "Unknown: Bit 14",                      // 0010 0000 0000 0000
    "Unknown: Bit 15",                      // 0100 0000 0000 0000
    "Unknown: Bit 16",                      // 1000 0000 0000 0000
};

void VictronComponent::dump_config() {  // NOLINT(google-readability-function-size,readability-function-size)
  ESP_LOGCONFIG(TAG, "Victron:");
  LOG_BINARY_SENSOR("  ", "Load state", load_state_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Relay state", relay_state_binary_sensor_);
  LOG_SENSOR("  ", "Max Power Yesterday", max_power_yesterday_sensor_);
  LOG_SENSOR("  ", "Max Power Today", max_power_today_sensor_);
  LOG_SENSOR("  ", "Yield Total", yield_total_sensor_);
  LOG_SENSOR("  ", "Yield Yesterday", yield_yesterday_sensor_);
  LOG_SENSOR("  ", "Yield Today", yield_today_sensor_);
  LOG_SENSOR("  ", "Panel Voltage", panel_voltage_sensor_);
  LOG_SENSOR("  ", "Panel Power", panel_power_sensor_);
  LOG_SENSOR("  ", "Battery Voltage", battery_voltage_sensor_);
  LOG_SENSOR("  ", "Battery Voltage 2", battery_voltage_2_sensor_);
  LOG_SENSOR("  ", "Battery Voltage 3", battery_voltage_3_sensor_);
  LOG_SENSOR("  ", "Battery Current", battery_current_sensor_);
  LOG_SENSOR("  ", "Battery Current", battery_current_2_sensor_);
  LOG_SENSOR("  ", "Battery Current", battery_current_3_sensor_);
  LOG_SENSOR("  ", "AC Out Voltage", ac_out_voltage_sensor_);
  LOG_SENSOR("  ", "AC Out Current", ac_out_current_sensor_);
  LOG_SENSOR("  ", "Load Current", load_current_sensor_);
  LOG_SENSOR("  ", "Day Number", day_number_sensor_);
  LOG_SENSOR("  ", "Charging Mode ID", charging_mode_id_sensor_);
  LOG_SENSOR("  ", "Error Code", error_code_sensor_);
  LOG_SENSOR("  ", "Warning Code", warning_code_sensor_);
  LOG_SENSOR("  ", "Tracking Mode ID", tracking_mode_id_sensor_);
  LOG_SENSOR("  ", "Device Mode ID", device_mode_id_sensor_);
  LOG_SENSOR("  ", "Off Reason Bitmask", off_reason_bitmask_sensor_);
  LOG_TEXT_SENSOR("  ", "Charging Mode", charging_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Error Text", error_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Warning Text", warning_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Tracking Mode", tracking_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Device Mode", device_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Firmware Version", firmware_version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Firmware Version 24bit", firmware_version_24bit_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Device Type", device_type_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Off Reason", off_reason_text_sensor_);

  LOG_SENSOR("  ", "Battery Temperature ", battery_temperature_sensor_);
  LOG_SENSOR("  ", "Instantaneous Power", instantaneous_power_sensor_);
  LOG_SENSOR("  ", "Consumed Amp Hours", consumed_amp_hours_sensor_);
  LOG_SENSOR("  ", "State Of Charge", state_of_charge_sensor_);
  LOG_SENSOR("  ", "Time To Go", time_to_go_sensor_);
  LOG_SENSOR("  ", "Depth Of The Deepest Discharge", depth_of_the_deepest_discharge_sensor_);
  LOG_SENSOR("  ", "Depth Of The Last Discharge", depth_of_the_last_discharge_sensor_);
  LOG_SENSOR("  ", "Number Of Charge Cycles", number_of_charge_cycles_sensor_);
  LOG_SENSOR("  ", "Number Of Full Discharges", number_of_full_discharges_sensor_);
  LOG_SENSOR("  ", "Min Battery Voltage", min_battery_voltage_sensor_);
  LOG_SENSOR("  ", "Max Battery Voltage", max_battery_voltage_sensor_);
  LOG_SENSOR("  ", "Last Full Charge", last_full_charge_sensor_);
  LOG_SENSOR("  ", "Amount Of Discharged Energy", amount_of_discharged_energy_sensor_);
  LOG_SENSOR("  ", "Amount Of Charged Energy", amount_of_charged_energy_sensor_);
  LOG_TEXT_SENSOR("  ", "Alarm Condition Active", alarm_condition_active_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Alarm Reason", alarm_reason_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Model Description", model_description_text_sensor_);

  check_uart_settings(19200);
}

void VictronComponent::loop() {
  const uint32_t now = millis();
  if ((state_ > 0) && (now - last_transmission_ >= 200)) {
    // last transmission too long ago. Reset RX index.
    ESP_LOGW(TAG, "Last transmission too long ago");
    state_ = 0;
  }

  int avail = available();
  if (avail <= 0)
    return;

  last_transmission_ = now;

  // Lecture par blocs (read_array) au lieu d'un read_byte() par octet : on evite
  // l'overhead d'un appel par caractere tout en gardant strictement la meme state
  // machine, appliquee ensuite sur le buffer local en memoire.
  static constexpr size_t BUF_SIZE = 128;
  uint8_t buf[BUF_SIZE];

  while ((avail = available()) > 0) {
    const size_t to_read = std::min(static_cast<size_t>(avail), BUF_SIZE);
    if (!read_array(buf, to_read)) {
      break;
    }

    for (size_t i = 0; i < to_read; i++) {
      const uint8_t c = buf[i];

      if (state_ == 0) {
        if (c == '\r' || c == '\n') {
          continue;
        }
        label_.clear();
        value_.clear();
        state_ = 1;
      }
      if (state_ == 1) {
        // Start of a ve.direct hex frame
        if (c == ':') {
          state_ = 3;
          continue;
        }
        if (c == '\t') {
          state_ = 2;
        } else {
          label_.push_back(c);
        }
        continue;
      }
      if (state_ == 2) {
        if (label_ == "Checksum") {
          state_ = 0;
          // The checksum is used as end of frame indicator
          if (now - this->last_publish_ >= this->throttle_) {
            this->last_publish_ = now;
            this->publishing_ = true;
          } else {
            this->publishing_ = false;
          }
          continue;
        }
        if (c == '\r' || c == '\n') {
          if (this->publishing_) {
            handle_value_();
          }
          state_ = 0;
        } else {
          value_.push_back(c);
        }
        continue;
      }
      // Discard ve.direct hex frame
      if (state_ == 3) {
        if (c == '\r' || c == '\n') {
          state_ = 0;
        }
      }
    }
  }
}

static std::string charging_mode_text(int value) {
  switch (value) {
    case 0:
      return "Off";
    case 1:
      return "Low power";
    case 2:
      return "Fault";
    case 3:
      return "Bulk";
    case 4:
      return "Absorption";
    case 5:
      return "Float";
    case 6:
      return "Storage";
    case 7:
      return "Equalize (manual)";
    case 9:
      return "Inverting";
    case 11:
      return "Power supply";
    case 245:
      return "Starting-up";
    case 246:
      return "Repeated absorption";
    case 247:
      return "Auto equalize / Recondition";
    case 248:
      return "BatterySafe";
    case 252:
      return "External control";
    default:
      return "Unknown";
  }
}

static std::string error_code_text(int value) {
  switch (value) {
    case 0:
      return "No error";
    case 2:
      return "Battery voltage too high";
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
    case 33:
      return "Input voltage too high (solar panel)";
    case 34:
      return "Input current too high (solar panel)";
    case 38:
      return "Input shutdown (excessive battery voltage)";
    case 39:
      return "Input shutdown (due to current flow during off mode)";
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
    default:
      return "Unknown";
  }
}

static std::string warning_code_text(int value) {
  switch (value) {
    case 0:
      return "No warning";
    case 1:
      return "Low Voltage";
    case 2:
      return "High Voltage";
    case 4:
      return "Low SOC";
    case 8:
      return "Low Starter Voltage";
    case 16:
      return "High Starter Voltage";
    case 32:
      return "Low Temperature";
    case 64:
      return "High Temperature";
    case 128:
      return "Mid Voltage";
    case 256:
      return "Overload";
    case 512:
      return "DC-ripple";
    case 1024:
      return "Low V AC out";
    case 2048:
      return "High V AC out";
    default:
      return "Multiple warnings";
  }
}

static std::string tracking_mode_text(int value) {
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

static std::string device_mode_text(int value) {
  switch (value) {
    case 0:
      return "Off";
    case 2:
      return "On";
    case 4:
      return "Off";
    case 5:
      return "Eco";
    default:
      return "Unknown";
  }
}

static std::string dc_monitor_mode_text(int value) {
  switch (value) {
    case -9:
      return "Solar charger";
    case -8:
      return "Wind turbine";
    case -7:
      return "Shaft generator";
    case -6:
      return "Alternator";
    case -5:
      return "Fuel cell";
    case -4:
      return "Water generator";
    case -3:
      return "DC/DC charger";
    case -2:
      return "AC charger";
    case -1:
      return "Generic source";
    case 0:
      return "Battery monitor (BMV)";
    case 1:
      return "Generic load";
    case 2:
      return "Electric drive";
    case 3:
      return "Fridge";
    case 4:
      return "Water pump";
    case 5:
      return "Bilge pump";
    case 6:
      return "DC system";
    case 7:
      return "Inverter";
    case 8:
      return "Water heater";
    default:
      return "Unknown";
  }
}

static std::string device_type_text(int value) {
  switch (value) {
    case 0x203:
      return "BMV-700";
    case 0x204:
      return "BMV-702";
    case 0x205:
      return "BMV-700H";
    case 0x0300:
      return "BlueSolar MPPT 70|15";
    case 0xA040:
      return "BlueSolar MPPT 75|50";
    case 0xA041:
      return "BlueSolar MPPT 150|35";
    case 0xA042:
      return "BlueSolar MPPT 75|15";
    case 0xA043:
      return "BlueSolar MPPT 100|15";
    case 0xA044:
      return "BlueSolar MPPT 100|30";
    case 0xA045:
      return "BlueSolar MPPT 100|50";
    case 0xA046:
      return "BlueSolar MPPT 150|70";
    case 0xA047:
      return "BlueSolar MPPT 150|100";
    case 0xA049:
      return "BlueSolar MPPT 100|50 rev2";
    case 0xA04A:
      return "BlueSolar MPPT 100|30 rev2";
    case 0xA04B:
      return "BlueSolar MPPT 150|35 rev2";
    case 0xA04C:
      return "BlueSolar MPPT 75|10";
    case 0xA04D:
      return "BlueSolar MPPT 150|45";
    case 0xA04E:
      return "BlueSolar MPPT 150|60";
    case 0xA04F:
      return "BlueSolar MPPT 150|85";
    case 0xA050:
      return "SmartSolar MPPT 250|100";
    case 0xA051:
      return "SmartSolar MPPT 150|100";
    case 0xA052:
      return "SmartSolar MPPT 150|85";
    case 0xA053:
      return "SmartSolar MPPT 75|15";
    case 0xA075:
      return "SmartSolar MPPT 75|15 rev2";
    case 0xA054:
      return "SmartSolar MPPT 75|10";
    case 0xA074:
      return "SmartSolar MPPT 75|10 rev2";
    case 0xA055:
      return "SmartSolar MPPT 100|15";
    case 0xA056:
      return "SmartSolar MPPT 100|30";
    case 0xA073:
      return "SmartSolar MPPT 150|45 rev3";
    case 0xA057:
      return "SmartSolar MPPT 100|50";
    case 0xA058:
      return "SmartSolar MPPT 150|35";
    case 0xA059:
      return "SmartSolar MPPT 150|100 rev2";
    case 0xA05A:
      return "SmartSolar MPPT 150|85 rev2";
    case 0xA05B:
      return "SmartSolar MPPT 250|70";
    case 0xA05C:
      return "SmartSolar MPPT 250|85";
    case 0xA05D:
      return "SmartSolar MPPT 250|60";
    case 0xA05E:
      return "SmartSolar MPPT 250|45";
    case 0xA05F:
      return "SmartSolar MPPT 100|20";
    case 0xA060:
      return "SmartSolar MPPT 100|20 48V";
    case 0xA061:
      return "SmartSolar MPPT 150|45";
    case 0xA062:
      return "SmartSolar MPPT 150|60";
    case 0xA063:
      return "SmartSolar MPPT 150|70";
    case 0xA064:
      return "SmartSolar MPPT 250|85 rev2";
    case 0xA065:
      return "SmartSolar MPPT 250|100 rev2";
    case 0xA066:
      return "BlueSolar MPPT 100|20";
    case 0xA067:
      return "BlueSolar MPPT 100|20 48V";
    case 0xA068:
      return "SmartSolar MPPT 250|60 rev2";
    case 0xA069:
      return "SmartSolar MPPT 250|70 rev2";
    case 0xA06A:
      return "SmartSolar MPPT 150|45 rev2";
    case 0xA06B:
      return "SmartSolar MPPT 150|60 rev2";
    case 0xA06C:
      return "SmartSolar MPPT 150|70 rev2";
    case 0xA06D:
      return "SmartSolar MPPT 150|85 rev3";
    case 0xA06E:
      return "SmartSolar MPPT 150|100 rev3";
    case 0xA06F:
      return "BlueSolar MPPT 150|45 rev2";
    case 0xA070:
      return "BlueSolar MPPT 150|60 rev2";
    case 0xA071:
      return "BlueSolar MPPT 150|70 rev2";
    case 0xA07D:
      return "BlueSolar MPPT 75|15 rev3";
    case 0xA102:
      return "SmartSolar MPPT VE.Can 150/70";
    case 0xA103:
      return "SmartSolar MPPT VE.Can 150/45";
    case 0xA104:
      return "SmartSolar MPPT VE.Can 150/60";
    case 0xA105:
      return "SmartSolar MPPT VE.Can 150/85";
    case 0xA106:
      return "SmartSolar MPPT VE.Can 150/100";
    case 0xA107:
      return "SmartSolar MPPT VE.Can 250/45";
    case 0xA108:
      return "SmartSolar MPPT VE.Can 250/60";
    case 0xA109:
      return "SmartSolar MPPT VE.Can 250/70";
    case 0xA10A:
      return "SmartSolar MPPT VE.Can 250/85";
    case 0xA10B:
      return "SmartSolar MPPT VE.Can 250/100";
    case 0xA10C:
      return "SmartSolar MPPT VE.Can 150/70 rev2";
    case 0xA10D:
      return "SmartSolar MPPT VE.Can 150/85 rev2";
    case 0xA10E:
      return "SmartSolar MPPT VE.Can 150/100 rev2";
    case 0xA10F:
      return "BlueSolar MPPT VE.Can 150/100";
    case 0xA112:
      return "BlueSolar MPPT VE.Can 250/70";
    case 0xA113:
      return "BlueSolar MPPT VE.Can 250/100";
    case 0xA114:
      return "SmartSolar MPPT VE.Can 250/70 rev2";
    case 0xA115:
      return "SmartSolar MPPT VE.Can 250/100 rev2";
    case 0xA116:
      return "SmartSolar MPPT VE.Can 250/85 rev2";
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
    case 0xA2F9:
      return "Phoenix Inverter 12V 1200VA 120V";
    case 0xA27A:
      return "Phoenix Inverter 24V 1200VA 120V";
    case 0xA27C:
      return "Phoenix Inverter 48V 1200VA 120V";
    case 0xA281:
      return "Phoenix Inverter 12V 1600VA 230V";
    case 0xA282:
      return "Phoenix Inverter 24V 1600VA 230V";
    case 0xA284:
      return "Phoenix Inverter 48V 1600VA 230V";
    case 0xA291:
      return "Phoenix Inverter 12V 2000VA 230V";
    case 0xA292:
      return "Phoenix Inverter 24V 2000VA 230V";
    case 0xA294:
      return "Phoenix Inverter 48V 2000VA 230V";
    case 0xA2A1:
      return "Phoenix Inverter 12V 3000VA 230V";
    case 0xA2A2:
      return "Phoenix Inverter 24V 3000VA 230V";
    case 0xA2A4:
      return "Phoenix Inverter 48V 3000VA 230V";
    case 0xA30A:
      return "Blue Smart IP65 Charger 12|25";
    case 0xA332:
      return "Blue Smart IP22 Charger 24|8";
    case 0xA334:
      return "Blue Smart IP22 Charger 24|12";
    case 0xA336:
      return "Blue Smart IP22 Charger 24|16";
    case 0xA340:
      return "Phoenix Smart IP43 Charger 12|50 (1+1)";
    case 0xA341:
      return "Phoenix Smart IP43 Charger 12|50 (3)";
    case 0xA342:
      return "Phoenix Smart IP43 Charger 24|25 (1+1)";
    case 0xA343:
      return "Phoenix Smart IP43 Charger 24|25 (3)";
    case 0xA344:
      return "Phoenix Smart IP43 Charger 12|30 (1+1)";
    case 0xA345:
      return "Phoenix Smart IP43 Charger 12|30 (3)";
    case 0xA346:
      return "Phoenix Smart IP43 Charger 24|16 (1+1)";
    case 0xA347:
      return "Phoenix Smart IP43 Charger 24|16 (3)";
    case 0xA381:
      return "BMV-712 Smart";
    case 0xA382:
      return "BMV-710H Smart";
    case 0xA383:
      return "BMV-712 Smart Rev2";
    case 0xA389:
      return "SmartShunt 500A/50mV";
    case 0xA38A:
      return "SmartShunt 1000A/50mV";
    case 0xA38B:
      return "SmartShunt 2000A/50mV";
    case 0xA442:
      return "Multi RS Solar 48V 6000VA 230V";
    // Additional PIDs mentioned in VE.Direct-HEX-Protocol specifications
    case 0xA2B1:
      return "Phoenix Inverter Smart 12V 5000VA 230Vac 64k";
    case 0xA2B2:
      return "Phoenix Inverter Smart 24V 5000VA 230Vac 64k";
    case 0xA2B4:
      return "Phoenix Inverter Smart 48V 5000VA 230Vac 64k";
    case 0xA2E1:
      return "Phoenix Inverter 12V 800VA 230Vac 64k HS";
    case 0xA2E2:
      return "Phoenix Inverter 24V 800VA 230Vac 64k HS";
    case 0xA2E4:
      return "Phoenix Inverter 48V 800VA 230Vac 64k HS";
    case 0xA2E9:
      return "Phoenix Inverter 12V 800VA 120Vac 64k HS";
    case 0xA2EA:
      return "Phoenix Inverter 24V 800VA 120Vac 64k HS";
    case 0xA2EC:
      return "Phoenix Inverter 48V 800VA 120Vac 64k HS";
    case 0xA2F1:
      return "Phoenix Inverter 12V 1200VA 230Vac 64k HS";
    case 0xA2F2:
      return "Phoenix Inverter 24V 1200VA 230Vac 64k HS";
    case 0xA2F4:
      return "Phoenix Inverter 48V 1200VA 230Vac 64k HS";
    case 0xA2FA:
      return "Phoenix Inverter 24V 1200VA 120Vac 64k HS";
    case 0xA2FC:
      return "Phoenix Inverter 48V 1200VA 120Vac 64k HS";
    case 0xA3F0:
      return "Orion XS 12V/12V-50A";
    case 0xA3F1:
      return "Orion XS 1400";
    case 0xA048:
      return "BlueSolar MPPT 75|50 rev2";
    case 0xA072:
      return "BlueSolar MPPT 150|45 rev3";
    case 0xA076:
      return "BlueSolar MPPT 100|30 rev3";
    case 0xA077:
      return "BlueSolar MPPT 100|50 rev3";
    case 0xA078:
      return "BlueSolar MPPT 150|35 rev2";
    case 0xA079:
      return "BlueSolar MPPT 75|10 rev2";
    case 0xA07A:
      return "BlueSolar MPPT 75|15 rev2";
    case 0xA07B:
      return "BlueSolar MPPT 100|15 rev2";
    case 0xA07C:
      return "BlueSolar MPPT 75/10 rev3";
    case 0xA07E:
      return "SmartSolar Charger MPPT 100/30";
    case 0xA110:
      return "SmartSolar MPPT RS 450/100";
    case 0xA111:
      return "SmartSolar MPPT RS 450/200";
    case 0xA117:
      return "BlueSolar MPPT VE.Can 150|100";
    default:
      return "Unknown";
  }
}

static std::string off_reason_text(uint32_t mask) {
  bool first = true;
  std::string value_list;

  if (mask) {
    for (uint8_t i = 0; i < OFF_REASONS_SIZE; i++) {
      if (mask & (1 << i)) {
        if (first) {
          first = false;
        } else {
          value_list.append(";");
        }
        value_list.append(OFF_REASONS[i]);
      }
    }
  }

  return value_list;
}

using Handler = std::function<void(VictronComponent &, const std::string &)>;

// Dispatch table construite une seule fois (static local a l'interieur d'une methode
// membre, pour garder l'acces aux membres protected des lambdas). Remplace la chaine
// de 64 comparaisons std::string sequentielles par une recherche par hash en O(1) amorti.
const std::unordered_map<std::string, Handler> &VictronComponent::get_handlers_() {
  static const std::unordered_map<std::string, Handler> handlers = {
      {"V", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.battery_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"V2", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.battery_voltage_2_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"V3", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.battery_voltage_3_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"VS", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.auxiliary_battery_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"VM", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.midpoint_voltage_of_the_battery_bank_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"DM", [](VictronComponent &c, const std::string &v) {
    // Per mill to %
    c.publish_state_(c.midpoint_deviation_of_the_battery_bank_sensor_, strtol(v.c_str(), nullptr, 10) * 0.10f);
    return;
      }},
      {"VPV", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.panel_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"PPV", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.panel_power_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"I", [](VictronComponent &c, const std::string &v) {
    // mA to A
    c.publish_state_(c.battery_current_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"I2", [](VictronComponent &c, const std::string &v) {
    // mA to A
    c.publish_state_(c.battery_current_2_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"I3", [](VictronComponent &c, const std::string &v) {
    // mA to A
    c.publish_state_(c.battery_current_3_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"IL", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.load_current_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"LOAD", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.load_state_binary_sensor_, v == "ON" || v == "On");
    return;
      }},
      {"T", [](VictronComponent &c, const std::string &v) {
    if (v == "---") {
      c.publish_state_(c.battery_temperature_sensor_, NAN);
      return;
    }

    c.publish_state_(c.battery_temperature_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"P", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.instantaneous_power_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"CE", [](VictronComponent &c, const std::string &v) {
    // mAh -> Ah
    c.publish_state_(c.consumed_amp_hours_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"SOC", [](VictronComponent &c, const std::string &v) {
    // Per mill to %
    c.publish_state_(c.state_of_charge_sensor_, strtol(v.c_str(), nullptr, 10) * 0.10f);
    return;
      }},
      {"TTG", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.time_to_go_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"Alarm", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.alarm_condition_active_text_sensor_, v);
    return;
      }},
      {"Relay", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.relay_state_binary_sensor_, v == "ON" || v == "On");
    return;
      }},
      {"AR", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.alarm_reason_text_sensor_, error_code_text(strtol(v.c_str(), nullptr, 10)));
    return;
      }},
      {"OR", [](VictronComponent &c, const std::string &v) {
    auto off_reason_bitmask = parse_hex<uint32_t>(v.substr(2, v.size() - 2));
    if (off_reason_bitmask) {
      c.publish_state_(c.off_reason_bitmask_sensor_, *off_reason_bitmask);
      c.publish_state_(c.off_reason_text_sensor_, off_reason_text(*off_reason_bitmask));
    }
    return;
      }},
      {"H1", [](VictronComponent &c, const std::string &v) {
    // mAh -> Ah
    c.publish_state_(c.depth_of_the_deepest_discharge_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0);
    return;
      }},
      {"H2", [](VictronComponent &c, const std::string &v) {
    // mAh -> Ah
    c.publish_state_(c.depth_of_the_last_discharge_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"H3", [](VictronComponent &c, const std::string &v) {
    // mAh -> Ah
    c.publish_state_(c.depth_of_the_average_discharge_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0);
    return;
      }},
      {"H4", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.number_of_charge_cycles_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"H5", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.number_of_full_discharges_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"H6", [](VictronComponent &c, const std::string &v) {
    if (v == "---") {
      c.publish_state_(c.cumulative_amp_hours_drawn_sensor_, NAN);
      return;
    }

    // mAh -> Ah
    c.publish_state_(c.cumulative_amp_hours_drawn_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"H7", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.min_battery_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"H8", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.max_battery_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"H9", [](VictronComponent &c, const std::string &v) {
    if (v == "---") {
      c.publish_state_(c.last_full_charge_sensor_, NAN);
      return;
    }

    // sec -> min
    c.publish_state_(c.last_full_charge_sensor_, (float) strtol(v.c_str(), nullptr, 10) / 60.0f);
    return;
      }},
      {"H10", [](VictronComponent &c, const std::string &v) {
    if (v == "---") {
      c.publish_state_(c.number_of_automatic_synchronizations_sensor_, NAN);
      return;
    }

    c.publish_state_(c.number_of_automatic_synchronizations_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"H11", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.number_of_low_main_voltage_alarms_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"H12", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.number_of_high_main_voltage_alarms_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"H13", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.number_of_low_auxiliary_voltage_alarms_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"H14", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.number_of_high_auxiliary_voltage_alarms_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"H15", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.min_auxiliary_battery_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"H16", [](VictronComponent &c, const std::string &v) {
    // mV to V
    c.publish_state_(c.max_auxiliary_battery_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 1000.0f);
    return;
      }},
      {"H17", [](VictronComponent &c, const std::string &v) {
// "H17"    0.01 kWh   Amount of discharged energy (BMV) / Amount of produced energy (DC monitor)
          // Wh
    c.publish_state_(c.amount_of_discharged_energy_sensor_, strtol(v.c_str(), nullptr, 10) * 10.0f);
    return;
      }},
      {"H18", [](VictronComponent &c, const std::string &v) {
// "H18"    0.01 kWh   Amount of charged energy (BMV) / Amount of consumed energy (DC monitor)
          // Wh
    c.publish_state_(c.amount_of_charged_energy_sensor_, strtol(v.c_str(), nullptr, 10) * 10.0f);
    return;
      }},
      {"H19", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.yield_total_sensor_, strtol(v.c_str(), nullptr, 10) * 10.0f);
    return;
      }},
      {"H20", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.yield_today_sensor_, strtol(v.c_str(), nullptr, 10) * 10.0f);
    return;
      }},
      {"H21", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.max_power_today_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"H22", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.yield_yesterday_sensor_, strtol(v.c_str(), nullptr, 10) * 10.0f);
    return;
      }},
      {"H23", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.max_power_yesterday_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"ERR", [](VictronComponent &c, const std::string &v) {
    int value = strtol(v.c_str(), nullptr, 10);
    c.publish_state_(c.error_code_sensor_, value);
    c.publish_state_(c.error_text_sensor_, error_code_text(value));
    return;
      }},
      {"CS", [](VictronComponent &c, const std::string &v) {
    int value = strtol(v.c_str(), nullptr, 10);
    c.publish_state_(c.charging_mode_id_sensor_, (float) value);
    c.publish_state_(c.charging_mode_text_sensor_, charging_mode_text(value));
    return;
      }},
      {"BMV", [](VictronComponent &c, const std::string &v) {
// "BMV"               Model description (deprecated)
          c.publish_state_(c.model_description_text_sensor_, v);
    return;
      }},
      {"FW", [](VictronComponent &c, const std::string &v) {
    std::string value = v;
    value.insert(value.size() - 2, ".");
    c.publish_state_once_(c.firmware_version_text_sensor_, value);
    return;
      }},
      {"FWE", [](VictronComponent &c, const std::string &v) {
    if (c.firmware_version_24bit_text_sensor_ == nullptr || c.firmware_version_24bit_text_sensor_->has_state())
      return;

    if (v.size() > 4) {
      std::string release_type = v.substr(v.size() - 2, 2);
      std::string version_number = v.substr(0, v.size() - 2);
      version_number = version_number.insert(version_number.size() - 2, ".");
      release_type = (release_type == "FF") ? "-official" : "-beta-" + release_type;

      c.publish_state_once_(c.firmware_version_24bit_text_sensor_, version_number + release_type);
      return;
    }

    c.publish_state_once_(c.firmware_version_24bit_text_sensor_, v);
    return;
      }},
      {"PID", [](VictronComponent &c, const std::string &v) {
    c.publish_state_once_(c.device_type_text_sensor_, device_type_text(strtol(v.c_str(), nullptr, 0)));
    return;
      }},
      {"SER#", [](VictronComponent &c, const std::string &v) {
    c.publish_state_once_(c.serial_number_text_sensor_, v);
    return;
      }},
      {"HC#", [](VictronComponent &c, const std::string &v) {
    c.publish_state_once_(c.hardware_revision_text_sensor_, v);
    return;
      }},
      {"HSDS", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.day_number_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"MODE", [](VictronComponent &c, const std::string &v) {
    int value = strtol(v.c_str(), nullptr, 10);
    c.publish_state_(c.device_mode_id_sensor_, (float) value);
    c.publish_state_(c.device_mode_text_sensor_, device_mode_text(value));
    return;
      }},
      {"AC_OUT_V", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.ac_out_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 100.0f);
    return;
      }},
      {"AC_OUT_I", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.ac_out_current_sensor_, std::max(0.0f, strtol(v.c_str(), nullptr, 10) / 10.0f));
    return;
      }},
      {"AC_OUT_S", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.ac_out_apparent_power_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
      {"WARN", [](VictronComponent &c, const std::string &v) {
    int value = strtol(v.c_str(), nullptr, 10);
    c.publish_state_(c.warning_code_sensor_, value);
    c.publish_state_(c.warning_text_sensor_, warning_code_text(value));
    return;
      }},
      {"MPPT", [](VictronComponent &c, const std::string &v) {
    int value = strtol(v.c_str(), nullptr, 10);
    c.publish_state_(c.tracking_mode_id_sensor_, (float) value);
    c.publish_state_(c.tracking_mode_text_sensor_, tracking_mode_text(value));
    return;
      }},
      {"MON", [](VictronComponent &c, const std::string &v) {
    int value = strtol(v.c_str(), nullptr, 10);
    c.publish_state_(c.dc_monitor_mode_id_sensor_, (float) value);
    c.publish_state_(c.dc_monitor_mode_text_sensor_, dc_monitor_mode_text(value));
    return;
      }},
      {"DC_IN_V", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.dc_input_voltage_sensor_, strtol(v.c_str(), nullptr, 10) / 100.0f);
    return;
      }},
      {"DC_IN_I", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.dc_input_current_sensor_, strtol(v.c_str(), nullptr, 10) / 10.0f);
    return;
      }},
      {"DC_IN_P", [](VictronComponent &c, const std::string &v) {
    c.publish_state_(c.dc_input_power_sensor_, strtol(v.c_str(), nullptr, 10));
    return;
      }},
  };
  return handlers;
}

void VictronComponent::handle_value_() {
  const auto &handlers = get_handlers_();
  auto it = handlers.find(label_);
  if (it != handlers.end()) {
    it->second(*this, value_);
    return;
  }
  ESP_LOGD(TAG, "Unhandled property: %s %s", label_.c_str(), value_.c_str());
}

void VictronComponent::publish_state_(binary_sensor::BinarySensor *binary_sensor, const bool &state) {
  if (binary_sensor == nullptr)
    return;

  binary_sensor->publish_state(state);
}

void VictronComponent::publish_state_(sensor::Sensor *sensor, float value) {
  if (sensor == nullptr)
    return;

  sensor->publish_state(value);
}

void VictronComponent::publish_state_(text_sensor::TextSensor *text_sensor, const std::string &state) {
  if (text_sensor == nullptr)
    return;

  text_sensor->publish_state(state);
}

void VictronComponent::publish_state_once_(text_sensor::TextSensor *text_sensor, const std::string &state) {
  if (text_sensor == nullptr)
    return;

  if (text_sensor->has_state())
    return;

  text_sensor->publish_state(state);
}

}  // namespace esphome::victron
