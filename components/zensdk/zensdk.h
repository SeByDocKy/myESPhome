#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_OUTPUT
#include "esphome/components/output/float_output.h"
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <string>
#include <vector>

namespace esphome {
namespace zensdk {

// ---------------------------------------------------------------------------
// Entity binding contracts between the Python codegen and the C++ hub.
//
// Every platform's __init__.py registers its entities with the hub together
// with the Zendure property name they map to (e.g. "electricLevel"), so the
// hub only needs generic tables -- there is no per-entity enum to keep in sync,
// except for the small NumberKind / SensorConv enums below, whose numeric
// values MUST match the KIND_* / CONV_* constants in the Python files.
// ---------------------------------------------------------------------------

// Value conversion applied by the hub before publishing a sensor.
enum SensorConv : uint8_t {
  CONV_LINEAR = 0,      // value * scale + offset
  CONV_DECIKELVIN = 1,  // (value - 2731) / 10 -> Celsius; 0 is treated as "no reading" and skipped
  CONV_INT16 = 2,       // low 16 bits reinterpreted as signed, then * scale
  CONV_VOLT_AUTO = 3,   // raw > 200 -> centivolts (raw / 100), otherwise volts (see README)
  CONV_TEMP_AUTO = 4,   // -> Celsius from 0.1 K (raw > 1000), plain K (raw > 200) or already Celsius; 0 skipped
  CONV_CELL_DELTA = 5,  // (maxVol - minVol) * 0.01 V of one pack; the binding's property is "maxVol"
};

// Text sensor conversion.
enum TextConv : uint8_t {
  TEXT_RAW = 0,    // string as-is (numbers are printed as integers)
  TEXT_STATE = 1,  // 0/1/2 -> Standby / Charging / Discharging
};

// What a number entity does when written.
enum NumberKind : uint8_t {
  NUM_PROPERTY = 0,       // plain integer property write
  NUM_SOC = 1,            // property write scaled by `soc_scale`
  NUM_INPUT_LIMIT = 2,    // AC charge power in W, sent as a full smart-mode charge command
  NUM_OUTPUT_LIMIT = 3,   // AC output power in W, sent as a full smart-mode discharge command
  NUM_POWER_SETPOINT = 4  // signed W: > 0 discharge, < 0 charge, 0 stop
};

#ifdef USE_SENSOR
struct SensorBinding {
  const char *prop;
  float scale;
  float offset;
  uint8_t conv;
  int8_t pack;  // -1: device-level property, >= 0: index into "packData"
  sensor::Sensor *sensor;
};
#endif
#ifdef USE_BINARY_SENSOR
struct BinaryBinding {
  const char *prop;
  binary_sensor::BinarySensor *sensor;
};
#endif
#ifdef USE_TEXT_SENSOR
struct TextBinding {
  const char *prop;
  uint8_t conv;
  int8_t pack;
  text_sensor::TextSensor *sensor;
};
#endif
#ifdef USE_NUMBER
struct NumberBinding {
  uint8_t kind;
  const char *prop;
  number::Number *number;
};
#endif
#ifdef USE_SELECT
struct SelectBinding {
  const char *prop;
  int32_t base;  // property value of option index 0
  select::Select *select;
};
#endif
#ifdef USE_SWITCH
struct SwitchBinding {
  const char *prop;
  switch_::Switch *sw;
};
#endif

// ---------------------------------------------------------------------------
// Background-task job/result plumbing
// ---------------------------------------------------------------------------
// All blocking HTTP I/O runs on a dedicated FreeRTOS task, never on ESPHome's
// cooperative main loop. update() and the control paths only enqueue a
// heap-allocated job; the task performs the request and posts a heap-allocated
// result back. loop() drains the result queue on the main thread and is the
// only place that touches ESPHome entities (they are not thread-safe).
enum JobType : uint8_t { JOB_POLL = 0, JOB_WRITE = 1 };

struct ZenSdkJob {
  JobType type{JOB_POLL};
  bool is_power{false};  // WRITE only: this was a charge/discharge command
  std::string body;      // WRITE only: complete JSON body
};

struct ZenSdkResult {
  JobType type{JOB_POLL};
  bool success{false};
  bool is_power{false};
  int status{0};
  std::string body;  // POLL only: response body
};

class ZenSdkComponent : public PollingComponent {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_sn(const std::string &sn) { this->sn_ = sn; }
  void set_max_charge_power(uint16_t w) { this->max_charge_power_ = w; }
  void set_max_discharge_power(uint16_t w) { this->max_discharge_power_ = w; }
  void set_soc_scale(uint8_t scale) { this->soc_scale_ = scale; }
  void set_write_flash_on_stop(bool v) { this->write_flash_on_stop_ = v; }

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

#ifdef USE_SENSOR
  void add_sensor(const char *prop, float scale, float offset, uint8_t conv, int8_t pack, sensor::Sensor *s) {
    this->sensors_.push_back({prop, scale, offset, conv, pack, s});
  }
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(const char *prop, binary_sensor::BinarySensor *s) { this->binary_sensors_.push_back({prop, s}); }
  void set_online_binary_sensor(binary_sensor::BinarySensor *s) { this->online_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void add_text_sensor(const char *prop, uint8_t conv, int8_t pack, text_sensor::TextSensor *s) {
    this->text_sensors_.push_back({prop, conv, pack, s});
  }
#endif
#ifdef USE_NUMBER
  void add_number(uint8_t kind, const char *prop, number::Number *n) { this->numbers_.push_back({kind, prop, n}); }
#endif
#ifdef USE_SELECT
  void add_select(const char *prop, int32_t base, select::Select *s) { this->selects_.push_back({prop, base, s}); }
#endif
#ifdef USE_SWITCH
  void add_switch(const char *prop, switch_::Switch *s) { this->switches_.push_back({prop, s}); }
#endif

  // ---- control API (main thread only; all fire-and-forget) ----

  // Signed AC power request in W: > 0 discharge, < 0 charge, 0 stop. Clamped to
  // the configured device limits. Always sends the complete smartMode/acMode/
  // inputLimit/outputLimit set (a bare limit write is ignored by the device once
  // it has dropped out of smart mode).
  void set_power(int32_t watts);

  // Plain integer property write, e.g. ("socSet", 900).
  void write_property(const char *prop, int32_t raw_value);

  // Used by the number platform; returns the effective (clamped) value.
  float write_number(uint8_t kind, const char *prop, float value);

  // Used by the two FloatOutputs (fraction 0..1 of the max charge / discharge
  // power). The effective request is (discharge - charge), so two independent
  // PID loops can drive one battery.
  void set_charge_request(float fraction);
  void set_discharge_request(float fraction);

 protected:
  std::string host_;
  std::string sn_;
  uint16_t port_{80};
  uint16_t max_charge_power_{0};
  uint16_t max_discharge_power_{0};
  uint8_t soc_scale_{10};
  bool write_flash_on_stop_{false};

  uint32_t http_id_{0};
  int32_t grid_off_power_{0};
  uint8_t consecutive_failures_{0};

  float charge_request_w_{0.0f};
  float discharge_request_w_{0.0f};
  std::string last_power_props_;
  bool last_power_valid_{false};
  uint32_t last_power_ms_{0};

  bool http_request_(bool post, const char *path, const std::string &body, int &status, std::string &response_body);
  static bool decode_chunked_(const std::string &in, std::string &out);
  static long parse_content_length_(const std::string &raw, size_t header_end);

  bool enqueue_write_(const std::string &props, bool is_power);
  void apply_output_requests_();
  bool parse_report_(const std::string &body);

  static void task_trampoline_(void *param);
  void run_task_();
  void process_result_(ZenSdkResult *result);

  QueueHandle_t job_queue_{nullptr};
  QueueHandle_t result_queue_{nullptr};
  TaskHandle_t task_handle_{nullptr};

#ifdef USE_SENSOR
  std::vector<SensorBinding> sensors_;
#endif
#ifdef USE_BINARY_SENSOR
  std::vector<BinaryBinding> binary_sensors_;
  binary_sensor::BinarySensor *online_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  std::vector<TextBinding> text_sensors_;
#endif
#ifdef USE_NUMBER
  std::vector<NumberBinding> numbers_;
#endif
#ifdef USE_SELECT
  std::vector<SelectBinding> selects_;
#endif
#ifdef USE_SWITCH
  std::vector<SwitchBinding> switches_;
#endif
};

#ifdef USE_NUMBER
class ZenSdkNumber : public number::Number, public Parented<ZenSdkComponent> {
 public:
  void set_kind(uint8_t kind) { this->kind_ = kind; }
  void set_property(const char *prop) { this->prop_ = prop; }

 protected:
  void control(float value) override {
    // Optimistic: publish what the hub actually sent (it may have clamped the value).
    this->publish_state(this->parent_->write_number(this->kind_, this->prop_, value));
  }
  uint8_t kind_{NUM_PROPERTY};
  const char *prop_{""};
};
#endif

#ifdef USE_SELECT
class ZenSdkSelect : public select::Select, public Parented<ZenSdkComponent> {
 public:
  void set_base(int32_t base) { this->base_ = base; }
  void set_property(const char *prop) { this->prop_ = prop; }

 protected:
  void control(size_t index) override {
    this->parent_->write_property(this->prop_, this->base_ + (int32_t) index);
    this->publish_state(index);
  }
  int32_t base_{0};
  const char *prop_{""};
};
#endif

#ifdef USE_SWITCH
// On/off property (e.g. "lampSwitch"): writes 1 / 0 and is read back on every poll.
// Not a Component, so no restore mode is applied: nothing is written to the device at boot.
class ZenSdkSwitch : public switch_::Switch, public Parented<ZenSdkComponent> {
 public:
  void set_property(const char *prop) { this->prop_ = prop; }

 protected:
  void write_state(bool state) override {
    this->parent_->write_property(this->prop_, state ? 1 : 0);
    this->publish_state(state);  // optimistic, corrected by the next poll if the device ignored it
  }
  const char *prop_{""};
};
#endif

#ifdef USE_OUTPUT
// 0.0 .. 1.0 maps to 0 .. max charge power of the device.
class ZenSdkChargeOutput : public output::FloatOutput, public Parented<ZenSdkComponent> {
 protected:
  void write_state(float state) override { this->parent_->set_charge_request(state); }
};

// 0.0 .. 1.0 maps to 0 .. max discharge power of the device.
class ZenSdkDischargeOutput : public output::FloatOutput, public Parented<ZenSdkComponent> {
 protected:
  void write_state(float state) override { this->parent_->set_discharge_request(state); }
};
#endif

}  // namespace zensdk
}  // namespace esphome
