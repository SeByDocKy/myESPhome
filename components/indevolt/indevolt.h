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
namespace indevolt {

// ---------------------------------------------------------------------------
// Entity binding contracts between the Python codegen and the C++ hub.
//
// Every platform's __init__.py registers its entities with the hub together with the device "point"
// (the numeric key of the INDEVOLT local API) they map to, so the hub only needs generic tables.
// The hub builds the list of points to poll from the registered entities, so only what is configured
// in the YAML is requested from the device. The small TextConv / NumberKind enums below MUST match the
// TEXT_* / NUM_* constants in the Python files.
// ---------------------------------------------------------------------------

// Text sensor conversion.
enum TextConv : uint8_t {
  TEXT_RAW = 0,        // string as-is (numbers are printed as integers)
  TEXT_FIRMWARE = 1,   // 5 digits "10203" -> "1.02.03", 3 digits "102" -> "1.02" (same as the HA integration)
  TEXT_WORK_MODE = 2,  // 7101: 0 Outdoor Portable, 1 Self-consumed Prioritized, 4 Real-Time Control, 5 Schedule
  TEXT_STATE = 3,      // 6001: 1000 Static, 1001 Charging, 1002 Discharging
  TEXT_ROLE = 4,       // 606: 1000 Master, 1001 Slave, 1002 None
};

// What a number entity does when written.
enum NumberKind : uint8_t {
  NUM_SETTING = 0,        // plain integer write to `write_point` (optionally read back from `read_point`)
  NUM_REALTIME_POWER = 1, // raw write of the real-time control power (point 47016), clamped to the device limits
  NUM_POWER_SETPOINT = 2, // signed W: > 0 discharge, < 0 charge, 0 standby -> full real-time command (point 47015)
  NUM_CHARGE_POWER = 3,   // charge power in W (>= 0) -> full real-time command; 0 only stops an ongoing charge
  NUM_DISCHARGE_POWER = 4,  // discharge power in W (>= 0) -> full real-time command; 0 only stops an ongoing discharge
};

// Identification values read once from GET /rpc/Sys.GetConfig.
enum InfoKind : uint8_t { INFO_TYPE = 0, INFO_SERIAL = 1, INFO_FIRMWARE = 2 };

#ifdef USE_SENSOR
struct SensorBinding {
  uint16_t point;
  float scale;
  float offset;
  sensor::Sensor *sensor;
};
#endif
#ifdef USE_BINARY_SENSOR
struct BinaryBinding {
  uint16_t point;
  int32_t on_value;  // published state is (value == on_value)
  binary_sensor::BinarySensor *sensor;
};
#endif
#ifdef USE_TEXT_SENSOR
struct TextBinding {
  uint16_t point;
  uint8_t conv;
  text_sensor::TextSensor *sensor;
};
struct InfoBinding {
  uint8_t kind;
  text_sensor::TextSensor *sensor;
};
#endif
#ifdef USE_NUMBER
struct NumberBinding {
  uint8_t kind;
  uint16_t read_point;   // 0: not readable (state is published optimistically when written)
  uint16_t write_point;
  number::Number *number;
};
#endif
#ifdef USE_SELECT
struct SelectBinding {
  uint16_t read_point;   // 0: not readable
  uint16_t write_point;
  int32_t read_offset;   // read-back: index value = raw - read_offset, looked up in `values`
  std::vector<int32_t> values;  // raw value written for each option index
  select::Select *select;
};
#endif
#ifdef USE_SWITCH
struct SwitchBinding {
  uint16_t read_point;
  uint16_t write_point;
  int32_t on_value;  // read-back: on when raw == on_value (writes are 1 / 0)
  switch_::Switch *sw;
};
#endif

// ---------------------------------------------------------------------------
// Background-task job/result plumbing
// ---------------------------------------------------------------------------
// All blocking HTTP I/O runs on a dedicated FreeRTOS task, never on ESPHome's cooperative main loop.
// update() and the control paths only enqueue a heap-allocated job; the task performs the requests and
// posts heap-allocated results back. loop() drains the result queue on the main thread and is the only
// place that touches ESPHome entities (they are not thread-safe).
//
// A polling cycle is a series of GetData calls of `batch_size` points each. The task runs the cycle one
// batch at a time and serves queued writes between two batches, so a control command never waits for a
// whole cycle. Three lists are polled (fast / normal / slow, see POLL_TIERS); the faster list is always served
// first. A failed batch aborts the cycle (an unreachable device costs one timeout, not one per batch).
enum JobType : uint8_t { JOB_POLL = 0, JOB_WRITE = 1, JOB_INFO = 2 };
enum ResultType : uint8_t { RES_BATCH = 0, RES_WRITE = 1, RES_INFO = 2 };

struct IndevoltJob {
  JobType type{JOB_POLL};
  bool is_power{false};  // WRITE only: this was a real-time power command
  uint8_t tier{0};       // POLL only: list to poll (0 fast, 1 normal, 2 slow)
  std::string path;      // WRITE: complete path + query of the SetData call
};

struct IndevoltResult {
  ResultType type{RES_BATCH};
  bool success{false};
  bool last{false};      // BATCH only: last batch of the cycle (or the cycle was aborted)
  uint8_t tier{0};       // BATCH only: list the batch belongs to
  bool aborted{false};   // BATCH only: cycle cancelled because another list found the device unreachable
  bool is_power{false};
  size_t batch{0};       // BATCH only
  int status{0};
  std::string body;
};

class IndevoltComponent : public PollingComponent {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_gen1(bool gen1) { this->gen1_ = gen1; }
  void set_timeout_ms(uint32_t ms) { this->timeout_ms_ = ms; }
  void set_batch_size(uint8_t n) { this->batch_size_ = n; }
  void set_normal_poll_interval(uint32_t ms) { this->tier_interval_ms_[1] = ms; }
  void set_slow_poll_interval(uint32_t ms) { this->tier_interval_ms_[2] = ms; }
  // Per-entity override of the default tier of a point (0 fast, 1 normal, 2 slow); the fastest request wins.
  void set_point_tier(uint16_t point, uint8_t tier) {
    for (auto &o : this->tier_overrides_) {
      if (o.first == point) {
        o.second = std::min(o.second, tier);
        return;
      }
    }
    this->tier_overrides_.emplace_back(point, tier);
  }
  void set_max_charge_power(uint16_t w) { this->max_charge_power_ = w; }
  void set_max_discharge_power(uint16_t w) { this->max_discharge_power_ = w; }
  void set_min_power(uint16_t w) { this->min_power_ = w; }
  void set_charge_target_soc(uint8_t soc) { this->charge_target_soc_ = soc; }
  void set_discharge_target_soc(uint8_t soc) { this->discharge_target_soc_ = soc; }
  void set_auto_realtime_mode(bool v) { this->auto_realtime_mode_ = v; }

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

#ifdef USE_SENSOR
  void add_sensor(uint16_t point, float scale, float offset, sensor::Sensor *s) {
    this->register_point_(point);
    this->sensors_.push_back({point, scale, offset, s});
  }
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(uint16_t point, int32_t on_value, binary_sensor::BinarySensor *s) {
    this->register_point_(point);
    this->binary_sensors_.push_back({point, on_value, s});
  }
  void set_online_binary_sensor(binary_sensor::BinarySensor *s) { this->online_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void add_text_sensor(uint16_t point, uint8_t conv, text_sensor::TextSensor *s) {
    this->register_point_(point);
    this->text_sensors_.push_back({point, conv, s});
  }
  void add_info_text_sensor(uint8_t kind, text_sensor::TextSensor *s) { this->info_sensors_.push_back({kind, s}); }
#endif
#ifdef USE_NUMBER
  void add_number(uint8_t kind, uint16_t read_point, uint16_t write_point, number::Number *n) {
    this->register_point_(read_point);
    this->numbers_.push_back({kind, read_point, write_point, n});
  }
#endif
#ifdef USE_SELECT
  void add_select(uint16_t read_point, uint16_t write_point, int32_t read_offset, const std::vector<int32_t> &values,
                  select::Select *s) {
    this->register_point_(read_point);
    this->selects_.push_back({read_point, write_point, read_offset, values, s});
  }
#endif
#ifdef USE_SWITCH
  void add_switch(uint16_t read_point, uint16_t write_point, int32_t on_value, switch_::Switch *s) {
    this->register_point_(read_point);
    this->switches_.push_back({read_point, write_point, on_value, s});
  }
#endif

  // ---- control API (main thread only; all fire-and-forget) ----

  // Signed power request in W: > 0 discharge, < 0 charge, 0 standby. Clamped to the configured limits;
  // a request smaller than `min_power` is sent as standby. Always sends the complete real-time command
  // [state, power, target SOC] to point 47015 and, when `auto_realtime_mode` is set, switches the device to
  // Real-Time Control first.
  // Returns the effective (clamped) request in W.
  int32_t set_power(int32_t watts);

  // Writes `values` to `point` (SetData, function code 16).
  void write_point(uint16_t point, const std::vector<int32_t> &values);

  // Used by the number platform; returns the effective (clamped) value.
  float write_number(uint8_t kind, uint16_t write_point, float value);

  // Used by the select platform when the work mode (point 47005) is changed by the user.
  void note_work_mode(int32_t mode) { this->work_mode_ = mode; }
  static constexpr uint16_t POINT_WORK_MODE_WRITE = 47005;

  // Used by the two FloatOutputs (fraction 0..1 of the max charge / discharge power). The effective
  // request is (discharge - charge), so two independent PID loops can drive one battery.
  void set_charge_request(float fraction);
  void set_discharge_request(float fraction);

 protected:
  std::string host_;
  uint16_t port_{8080};
  bool gen1_{false};
  uint32_t timeout_ms_{8000};
  uint8_t batch_size_{8};
  uint16_t max_charge_power_{0};
  uint16_t max_discharge_power_{0};
  uint16_t min_power_{0};
  uint8_t charge_target_soc_{100};
  uint8_t discharge_target_soc_{5};
  bool auto_realtime_mode_{true};

  // Points to poll (deduplicated, in registration order). Built before setup(), read-only afterwards.
  std::vector<uint16_t> read_points_;
  void register_point_(uint16_t point);

  // read_points_ split in three lists, each polled on its own timer: fast (fast_poll_interval: power, SOC,
  // state, work mode), normal (normal_poll_interval: measurements and counters) and slow (slow_poll_interval:
  // near-static data, settings read-back, identification). The default tier of a point comes from a table in
  // the .cpp, `poll:` on an entity overrides it.
  static constexpr uint8_t TIERS = 3;
  std::vector<uint16_t> tier_points_[TIERS];
  std::vector<std::pair<uint16_t, uint8_t>> tier_overrides_;
  uint32_t tier_interval_ms_[TIERS]{30000, 60000, 300000};
  const std::vector<uint16_t> &points_(uint8_t tier) const { return this->tier_points_[tier]; }
  void start_poll_(uint8_t tier);

  int32_t work_mode_{-1};  // last known work mode (point 7101 / 47005); -1 = unknown
  uint8_t consecutive_failures_{0};
  uint8_t empty_batch_warnings_{0};
  bool info_done_{false};
  bool info_pending_{false};
  bool poll_pending_[TIERS]{false, false, false};
  uint32_t poll_started_ms_[TIERS]{0, 0, 0};

  float charge_request_w_{0.0f};
  float discharge_request_w_{0.0f};
  std::string last_power_command_;
  bool last_power_valid_{false};
  uint32_t last_power_ms_{0};

  size_t batch_count_(uint8_t tier) const;
  std::string get_data_path_(uint8_t tier, size_t batch) const;
  std::string set_data_path_(uint16_t point, const std::vector<int32_t> &values) const;
  bool enqueue_write_(const std::string &path, bool is_power);
  void enqueue_info_();
  void apply_output_requests_();
#ifdef USE_NUMBER
  void publish_power_numbers_(int32_t watts);
#endif
  int32_t requested_w_{0};  // last effective power request (> 0 discharge, < 0 charge)

  bool http_request_(bool post, const std::string &path, int &status, std::string &response_body);
  static bool decode_chunked_(const std::string &in, std::string &out);
  static long parse_content_length_(const std::string &raw, size_t header_end);

  bool parse_batch_(uint8_t tier, size_t batch, const std::string &body);
  void parse_info_(const std::string &body);
  void process_result_(IndevoltResult *result);

  static void task_trampoline_(void *param);
  void run_task_();

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
  std::vector<InfoBinding> info_sensors_;
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
class IndevoltNumber : public number::Number, public Parented<IndevoltComponent> {
 public:
  void set_kind(uint8_t kind) { this->kind_ = kind; }
  void set_write_point(uint16_t point) { this->write_point_ = point; }

 protected:
  void control(float value) override {
    // Optimistic: publish what the hub actually sent (it may have clamped the value).
    this->publish_state(this->parent_->write_number(this->kind_, this->write_point_, value));
  }
  uint8_t kind_{NUM_SETTING};
  uint16_t write_point_{0};
};
#endif

#ifdef USE_SELECT
class IndevoltSelect : public select::Select, public Parented<IndevoltComponent> {
 public:
  void set_write_point(uint16_t point) { this->write_point_ = point; }
  void set_values(const std::vector<int32_t> &values) { this->values_ = values; }

 protected:
  void control(size_t index) override {
    if (index >= this->values_.size())
      return;
    int32_t raw = this->values_[index];
    this->parent_->write_point(this->write_point_, {raw});
    if (this->write_point_ == IndevoltComponent::POINT_WORK_MODE_WRITE)
      this->parent_->note_work_mode(raw);
    this->publish_state(index);
  }
  uint16_t write_point_{0};
  std::vector<int32_t> values_;
};
#endif

#ifdef USE_SWITCH
// On/off point (e.g. light, grid charging, bypass): writes 1 / 0 and is read back on every poll.
// Not a Component, so no restore mode is applied: nothing is written to the device at boot.
class IndevoltSwitch : public switch_::Switch, public Parented<IndevoltComponent> {
 public:
  void set_write_point(uint16_t point) { this->write_point_ = point; }

 protected:
  void write_state(bool state) override {
    this->parent_->write_point(this->write_point_, {state ? 1 : 0});
    this->publish_state(state);  // optimistic, corrected by the next poll if the device ignored it
  }
  uint16_t write_point_{0};
};
#endif

#ifdef USE_OUTPUT
// 0.0 .. 1.0 maps to 0 .. max charge power of the device.
class IndevoltChargeOutput : public output::FloatOutput, public Parented<IndevoltComponent> {
 protected:
  void write_state(float state) override { this->parent_->set_charge_request(state); }
};

// 0.0 .. 1.0 maps to 0 .. max discharge power of the device.
class IndevoltDischargeOutput : public output::FloatOutput, public Parented<IndevoltComponent> {
 protected:
  void write_state(float state) override { this->parent_->set_discharge_request(state); }
};
#endif

}  // namespace indevolt
}  // namespace esphome
