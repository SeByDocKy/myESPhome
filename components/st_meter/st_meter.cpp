#include "st_meter.h"
#include "st_meter_registers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st_meter {

static const char *const TAG = "st_meter";
uint16_t ph_v, ph_c, ph_ap, ph_rp, ph_pf = 0x0000;
static const uint8_t MODBUS_CMD_READ_IN_REGISTERS = 0x04;
static const uint8_t MODBUS_REGISTER_COUNT = 80;  // 74 x 16-bit registers

void STMeter::on_modbus_data(const std::vector<uint8_t> &data) {
  if (data.size() < MODBUS_REGISTER_COUNT * 2) {
    ESP_LOGW(TAG, "Invalid size for STMeter!");
    return;
  }

  auto st_meter_get_float = [&](size_t i) -> float {
    uint32_t temp = encode_uint32(data[i], data[i + 1], data[i + 2], data[i + 3]);
    float f;
    memcpy(&f, &temp, sizeof(f));
    return f;
  };

  for (uint8_t i = 0; i < 3; i++) {
    auto phase = this->phases_[i];
    if (!phase.setup)
      continue;
	
	if(i==0) {
		ph_v = ST_PHASE_1_VOLTAGE;
		ph_c = ST_PHASE_1_CURRENT;
		ph_ap = ST_PHASE_1_ACTIVE_POWER;
		ph_rp = ST_PHASE_1_REACTIVE_POWER;
		ph_pf = ST_PHASE_1_POWER_FACTOR;
	}
	if(i==1)  {
		ph_v = ST_PHASE_2_VOLTAGE;
		ph_c = ST_PHASE_2_CURRENT;
		ph_ap = ST_PHASE_2_ACTIVE_POWER;
		ph_rp = ST_PHASE_2_REACTIVE_POWER;
		ph_pf = ST_PHASE_2_POWER_FACTOR;
	}
	if(i==2)  {
		ph_v = ST_PHASE_3_VOLTAGE;
		ph_c = ST_PHASE_3_CURRENT;
		ph_ap = ST_PHASE_3_ACTIVE_POWER;
		ph_rp = ST_PHASE_3_REACTIVE_POWER;
		ph_pf = ST_PHASE_3_POWER_FACTOR;
	}
	 
    float voltage = st_meter_get_float(ph_v * 2 + (i * 4));
    float current = st_meter_get_float(ph_c * 2 + (i * 4));
    float active_power = st_meter_get_float(ph_ap * 2 + (i * 4));
    float reactive_power = st_meter_get_float(ph_rp * 2 + (i * 4));
    float power_factor = st_meter_get_float(ph_pf * 2 + (i * 4));

    ESP_LOGD(
        TAG,
        "STMeter Phase %c: V=%.3f V, I=%.3f A, Active P=%.3f W, Reactive P=%.3f VAR, PF=%.3f",
        i + 'A', voltage, current, active_power, reactive_power, power_factor);
    if (phase.voltage_sensor_ != nullptr)
      phase.voltage_sensor_->publish_state(voltage);
    if (phase.current_sensor_ != nullptr)
      phase.current_sensor_->publish_state(current);
    if (phase.active_power_sensor_ != nullptr)
      phase.active_power_sensor_->publish_state(active_power);
    if (phase.reactive_power_sensor_ != nullptr)
      phase.reactive_power_sensor_->publish_state(reactive_power);
    if (phase.power_factor_sensor_ != nullptr)
      phase.power_factor_sensor_->publish_state(power_factor);
  }

  float frequency = st_meter_get_float(ST_FREQUENCY * 2);
  float total_active_power = st_meter_get_float(ST_TOTAL_ACTIVE_POWER * 2);
  float total_active_electricity_power = st_meter_get_float(ST_TOTAL_ACTIVE_ELECTRICITY_POWER * 2);
  float total_reactive_power = st_meter_get_float(ST_TOTAL_REACTIVE_POWER * 2);
  float total_reactive_electricity_power = st_meter_get_float(ST_TOTAL_REACTIVE_ELECTRICITY_POWER * 2);

  ESP_LOGD(TAG, "STMeter: F=%.3f Hz, T.A.P=%.3f Wh, T.A.E.P=%.3f Wh, T.R.P=%.3f VARh, T.R.E.P=%.3f VARh",
           frequency, total_active_power, total_active_electricity_power, total_reactive_power, total_reactive_electricity_power);

  if (this->frequency_sensor_ != nullptr)
    this->frequency_sensor_->publish_state(frequency);
  if (this->total_active_power_sensor_ != nullptr)
    this->total_active_power_sensor_->publish_state(total_active_power);
  if (this->total_active_electricity_power_sensor_ != nullptr)
    this->total_active_electricity_power_sensor_->publish_state(total_active_electricity_power);
  if (this->total_reactive_power_sensor_ != nullptr)
    this->total_reactive_power_sensor_->publish_state(total_reactive_power);
  if (this->total_reactive_electricity_power_sensor_ != nullptr)
    this->total_reactive_electricity_power_sensor_->publish_state(total_reactive_electricity_power);
}

void STMeter::update() { this->send(MODBUS_CMD_READ_IN_REGISTERS, 0, MODBUS_REGISTER_COUNT); }
void STMeter::dump_config() {
  ESP_LOGCONFIG(TAG, "ST Meter:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  for (uint8_t i = 0; i < 3; i++) {
    auto phase = this->phases_[i];
    if (!phase.setup)
      continue;
    ESP_LOGCONFIG(TAG, "  Phase %c", i + 'A');
    LOG_SENSOR("    ", "Voltage", phase.voltage_sensor_);
    LOG_SENSOR("    ", "Current", phase.current_sensor_);
    LOG_SENSOR("    ", "Active Power", phase.active_power_sensor_);
    LOG_SENSOR("    ", "Reactive Power", phase.reactive_power_sensor_);
    LOG_SENSOR("    ", "Power Factor", phase.power_factor_sensor_);
  }
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Total Active Power", this->total_active_power_sensor_);
  LOG_SENSOR("  ", "Total Active Electricity Power", this->total_active_electricity_power_sensor_);
  LOG_SENSOR("  ", "Total Reactive Power", this->total_reactive_power_sensor_);
  LOG_SENSOR("  ", "Total Reactive Electricity Power", this->total_reactive_electricity_power_sensor_);
}

}  // namespace st_meter
}  // namespace esphome
