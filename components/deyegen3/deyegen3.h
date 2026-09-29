#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_OUTPUT
#include "esphome/components/output/float_output.h"
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace deyegen3 {

// ---------------------------------------------------------------------------
// Solarman V5 framing constants -- identical transport to this author's
// tsungen3 (TSUN GEN3 PLUS) component; see that component's README for the
// protocol references. Only the Modbus register map below differs by brand.
// ---------------------------------------------------------------------------
static const uint8_t V5_START = 0xA5;
static const uint8_t V5_END = 0x15;
static const uint16_t V5_CTRL_REQUEST = 0x4510;
static const uint16_t V5_CTRL_RESPONSE = 0x1510;

static const uint8_t MB_READ_HOLDING_REGISTERS = 0x03;
static const uint8_t MB_WRITE_SINGLE_REGISTER = 0x06;

static const uint8_t V5_FRAME_TYPE_INVERTER = 0x02;
static const uint16_t V5_SENSOR_TYPE_MODBUS = 0x0000;

// ---------------------------------------------------------------------------
// Deye GEN3 microinverter register map
// ---------------------------------------------------------------------------
// Sourced from https://github.com/StephanJoubert/home_assistant_solarman
// (custom_components/solarman/inverter_definitions/deye_2mppt.yaml and
// deye_4mppt.yaml), NOT from real hardware -- this component was written
// "blind" (no Deye unit available at the time). See README.md's "Blind
// implementation" section before trusting any single value. The register
// addresses themselves come straight from that repo; the *interpretation*
// of multi-register ("rule 3") 32-bit fields as high-word-first, and of
// the "Inverter ID" string's byte order, are this author's best-guess
// extrapolation from common Modbus/Solarman convention -- both may be wrong
// and need correcting against a packet capture or real readings.
static const uint16_t REG_BLOCK_START = 0x0001;
static const uint16_t REG_BLOCK_COUNT = 0x7D;  // 0x0001..0x007D inclusive (125 regs, the FC03 request in the source project)

static const uint16_t REG_INVERTER_ID_START = 0x0003;  // 5 registers, ASCII string (e.g. "SUN800G3..." -- unconfirmed)
static const uint8_t REG_INVERTER_ID_COUNT = 5;
static const uint16_t REG_RATED_POWER = 0x0010;         // 0.1 W
static const uint16_t REG_ACTIVE_POWER_REGULATION = 0x0028;  // 1 % per unit, write target for power_percent (FC06)
static const uint16_t REG_DAILY_PRODUCTION = 0x003C;    // 0.1 kWh
static const uint16_t REG_RUNNING_STATUS = 0x003B;      // enum: 0=Stand-by 1=Self-check 2=Normal 3=Warning 4=Fault
static const uint16_t REG_TOTAL_PRODUCTION = 0x003F;    // + 0x0040, 32-bit, 0.1 kWh
static const uint16_t REG_AC_VOLTAGE = 0x0049;           // 0.1 V
static const uint16_t REG_GRID_CURRENT = 0x004C;         // 0.1 A, signed
static const uint16_t REG_AC_FREQUENCY = 0x004F;         // 0.01 Hz
static const uint16_t REG_TOTAL_AC_POWER = 0x0056;       // + 0x0057, 32-bit, 0.1 W
static const uint16_t REG_RADIATOR_TEMP = 0x005A;        // (raw - 1000) * 0.01 degC
static const uint16_t REG_PV1_VOLTAGE = 0x006D;          // 0.1 V
static const uint16_t REG_PV1_CURRENT = 0x006E;          // 0.1 A
static const uint16_t REG_PV2_VOLTAGE = 0x006F;
static const uint16_t REG_PV2_CURRENT = 0x0070;
static const uint16_t REG_PV3_VOLTAGE = 0x0071;          // 4-MPPT variant only
static const uint16_t REG_PV3_CURRENT = 0x0072;
static const uint16_t REG_PV4_VOLTAGE = 0x0073;
static const uint16_t REG_PV4_CURRENT = 0x0074;

// No PV per-string power register exists in the source register map -- pv*_power
// is computed in software as voltage * current, unlike tsungen3 where the
// inverter reports it directly.
//
// No alarm/fault bitmask register is known for this protocol (unlike tsungen3's
// event_alarms/event_faults) -- "Running Status" (Warning/Fault) is the only
// health indication available and is what inverter_status publishes.
//
// No AT+ or reset-equivalent command is known for this protocol either.

enum class DeyeGen3Model {
  MODEL_AUTO,
  MODEL_2MPPT,
  MODEL_4MPPT,
};

class DeyeGen3Component : public PollingComponent {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_modbus_address(uint8_t address) { this->modbus_address_ = address; }
  void set_logger_serial(uint32_t serial) { this->logger_serial_ = serial; }
  void set_model(DeyeGen3Model model) { this->configured_model_ = model; }

  void setup() override;
  void update() override;
  // Safe to override: PollingComponent schedules update() through ESPHome's
  // internal scheduler, not through loop() -- see tsungen3 for the same note
  // and its confirmation against esphome/core/component.h.
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Writes the Active Power Regulations register (percent of rated power,
  // 0-100). Called by the `number` and `output` platforms. Fire-and-forget:
  // enqueues a job for the background task and returns almost immediately.
  void set_power_percent(float percent);

#ifdef USE_SENSOR
  void set_grid_voltage_sensor(sensor::Sensor *s) { this->grid_voltage_sensor_ = s; }
  void set_grid_current_sensor(sensor::Sensor *s) { this->grid_current_sensor_ = s; }
  void set_grid_frequency_sensor(sensor::Sensor *s) { this->grid_frequency_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_rated_power_sensor(sensor::Sensor *s) { this->rated_power_sensor_ = s; }
  void set_current_power_sensor(sensor::Sensor *s) { this->current_power_sensor_ = s; }
  void set_ac_energy_today_sensor(sensor::Sensor *s) { this->ac_energy_today_sensor_ = s; }
  void set_ac_energy_total_sensor(sensor::Sensor *s) { this->ac_energy_total_sensor_ = s; }

  void set_pv1_voltage_sensor(sensor::Sensor *s) { this->pv_voltage_sensor_[0] = s; }
  void set_pv1_current_sensor(sensor::Sensor *s) { this->pv_current_sensor_[0] = s; }
  void set_pv1_power_sensor(sensor::Sensor *s) { this->pv_power_sensor_[0] = s; }
  void set_pv2_voltage_sensor(sensor::Sensor *s) { this->pv_voltage_sensor_[1] = s; }
  void set_pv2_current_sensor(sensor::Sensor *s) { this->pv_current_sensor_[1] = s; }
  void set_pv2_power_sensor(sensor::Sensor *s) { this->pv_power_sensor_[1] = s; }
  void set_pv3_voltage_sensor(sensor::Sensor *s) { this->pv_voltage_sensor_[2] = s; }
  void set_pv3_current_sensor(sensor::Sensor *s) { this->pv_current_sensor_[2] = s; }
  void set_pv3_power_sensor(sensor::Sensor *s) { this->pv_power_sensor_[2] = s; }
  void set_pv4_voltage_sensor(sensor::Sensor *s) { this->pv_voltage_sensor_[3] = s; }
  void set_pv4_current_sensor(sensor::Sensor *s) { this->pv_current_sensor_[3] = s; }
  void set_pv4_power_sensor(sensor::Sensor *s) { this->pv_power_sensor_[3] = s; }
#endif

#ifdef USE_TEXT_SENSOR
  void set_inverter_status_text_sensor(text_sensor::TextSensor *s) { this->inverter_status_text_sensor_ = s; }
#endif

 protected:
  std::string host_;
  uint16_t port_{8899};
  uint8_t modbus_address_{1};
  uint32_t logger_serial_{0};
  uint8_t v5_serial_{0};
  DeyeGen3Model configured_model_{DeyeGen3Model::MODEL_AUTO};
  // Effective model actually used to decide whether PV3/PV4 are published --
  // resolved once at setup() from configured_model_, or guessed from the
  // Inverter ID string on the first successful poll if configured_model_ is
  // MODEL_AUTO. Defaults conservatively to 2 MPPT (no PV3/PV4) until then.
  DeyeGen3Model effective_model_{DeyeGen3Model::MODEL_2MPPT};
  bool model_resolved_{false};

  bool connect_and_transact_(const std::vector<uint8_t> &request, std::vector<uint8_t> &response);

  std::vector<uint8_t> wrap_v5_request_(uint8_t frame_type, uint16_t sensor_type, const std::vector<uint8_t> &tail);
  bool extract_v5_payload_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &payload);

  std::vector<uint8_t> build_read_request_(uint16_t start_reg, uint16_t count);
  bool parse_response_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &register_data);
  void handle_live_block_(const std::vector<uint8_t> &regs, uint16_t start_reg, uint16_t count);
  // Reads the "Inverter ID" string out of an already-decoded register block
  // and, if configured_model_ is MODEL_AUTO, updates effective_model_ by
  // looking for a 4-MPPT model number ("1300"/"1600"/"2000") in it.
  // Best-guess parsing, not confirmed against a real string -- see README.
  void resolve_model_(const std::vector<uint8_t> &regs, uint16_t start_reg);

  std::vector<uint8_t> build_write_request_(uint16_t reg, uint16_t value);
  bool parse_write_response_(const std::vector<uint8_t> &frame, uint16_t expected_reg, uint16_t expected_value);

  static uint16_t modbus_crc16_(const uint8_t *data, size_t len);
  static uint8_t v5_checksum_(const uint8_t *data, size_t len);

  static uint16_t get_u16_(const std::vector<uint8_t> &regs, uint16_t reg, uint16_t start_reg);
  static int16_t get_s16_(const std::vector<uint8_t> &regs, uint16_t reg, uint16_t start_reg);
  // High-word-first, per register pair as listed in the source project's
  // "rule 3" fields (e.g. [0x003F, 0x0040] -> 0x003F is the high word). This
  // is the opposite convention from tsungen3's TSUN registers (low-word-first)
  // -- unverified for Deye, flagged in README as a value to double-check.
  static uint32_t get_u32_hi_first_(const std::vector<uint8_t> &regs, uint16_t reg_hi, uint16_t reg_lo,
                                     uint16_t start_reg);

  static const char *running_status_name_(uint16_t value);

  // Background-task plumbing -- identical pattern to tsungen3, adopted from
  // the start this time rather than retrofitted: all blocking TCP I/O runs on
  // a dedicated FreeRTOS task, and only loop() (main thread) ever touches
  // Sensor/TextSensor objects or calls publish_state()/ESP_LOGx for results.
  enum class JobType {
    POLL_READ,
    WRITE_POWER_PERCENT,
  };
  struct Job {
    JobType type;
    uint16_t reg_value{0};
  };
  struct JobResult {
    JobType type;
    bool success{false};
    uint16_t reg_value{0};
    std::vector<uint8_t> register_data;
  };

  static void task_trampoline_(void *param);
  void run_task_();
  void process_result_(JobResult *result);

  QueueHandle_t job_queue_{nullptr};
  QueueHandle_t result_queue_{nullptr};
  TaskHandle_t task_handle_{nullptr};

#ifdef USE_SENSOR
  sensor::Sensor *grid_voltage_sensor_{nullptr};
  sensor::Sensor *grid_current_sensor_{nullptr};
  sensor::Sensor *grid_frequency_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *rated_power_sensor_{nullptr};
  sensor::Sensor *current_power_sensor_{nullptr};
  sensor::Sensor *ac_energy_today_sensor_{nullptr};
  sensor::Sensor *ac_energy_total_sensor_{nullptr};
  sensor::Sensor *pv_voltage_sensor_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *pv_current_sensor_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *pv_power_sensor_[4]{nullptr, nullptr, nullptr, nullptr};
#endif

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *inverter_status_text_sensor_{nullptr};
#endif
};

#ifdef USE_NUMBER
// power_percent: sets the Active Power Regulations register (max output
// power, as a percent of Rated Power). Optimistic: publishes the requested
// value right away rather than waiting to read it back on the next poll.
class DeyeGen3PowerPercentNumber : public number::Number, public Parented<DeyeGen3Component> {
 protected:
  void control(float value) override {
    this->parent_->set_power_percent(value);
    this->publish_state(value);
  }
};
#endif

#ifdef USE_OUTPUT
// Same underlying write as the number above, exposed as a plain FloatOutput
// (0.0-1.0) for use as a write_action target from other components/automations.
class DeyeGen3PowerPercentOutput : public output::FloatOutput, public Parented<DeyeGen3Component> {
 protected:
  void write_state(float state) override { this->parent_->set_power_percent(state * 100.0f); }
};
#endif

}  // namespace deyegen3
}  // namespace esphome
