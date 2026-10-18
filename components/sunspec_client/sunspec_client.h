#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/modbus/modbus.h"

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace esphome::sunspec_client {

// "SunS" marker, two registers: 0x5375 ("Su") then 0x6E53 ("nS")
static constexpr uint16_t SUNSPEC_MARKER_HI = 0x5375;
static constexpr uint16_t SUNSPEC_MARKER_LO = 0x6E53;
// Model id that ends the chain of models
static constexpr uint16_t SUNSPEC_END_MODEL_ID = 0xFFFF;
// Safety cap on the number of models read during a scan
static constexpr uint8_t MAX_SCANNED_MODELS = 64;
// Retries of a request that got no answer, before the scan is declared failed
static constexpr uint8_t MAX_NO_RESPONSE_RETRIES = 3;
// A text sensor state is limited to 255 characters in Home Assistant
static constexpr size_t MAX_TEXT_STATE_LENGTH = 250;

/// One model found during the scan.
struct ModelInfo {
  uint16_t id;       ///< SunSpec model id
  uint16_t length;   ///< number of data registers (header excluded)
  uint16_t address;  ///< register address of the model header
};

enum class ScanState : uint8_t {
  IDLE,           ///< no scan running (not started yet, finished or failed)
  PROBING_BASE,   ///< looking for the "SunS" marker at the candidate base addresses
  READING_HEADER  ///< walking the chain of model headers
};

/// SunSpec client on the core modbus bus.
///
/// The component is itself a modbus client device. A scan walks the chain of model headers (one request of
/// two registers per model) and publishes the list of model ids in the `models` text sensor. All requests
/// go through the hub queue, one at a time, and replies arrive in loop() on the main task.
class SunSpecClient final : public Component, public modbus::ModbusClientDevice {
 public:
  void setup() override;
  void dump_config() override;
  /// Late: the Modbus bus and the link below it (WiFi, TCP) are set up first.
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_startup_delay(uint32_t delay_ms) { this->startup_delay_ms_ = delay_ms; }
  void set_scan_retry_interval(uint32_t interval_ms) { this->scan_retry_interval_ms_ = interval_ms; }
  void set_name_prefix(const std::string &prefix) { this->name_prefix_ = prefix; }
  void add_wanted_model(uint16_t model_id) { this->wanted_models_.push_back(model_id); }

#ifdef USE_TEXT_SENSOR
  void set_models_text_sensor(text_sensor::TextSensor *sensor) { this->models_text_sensor_ = sensor; }
#endif

  /// Models found by the last successful scan (empty until then).
  const std::vector<ModelInfo> &get_models() const { return this->models_; }
  bool is_scan_done() const { return this->scan_done_; }

  void on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                 modbus::ResponseStatus status) override;
  bool on_no_response(std::span<const uint8_t> request_pdu) override;

 protected:
  void start_scan_();
  void probe_base_();
  void read_header_(uint16_t address);
  bool request_(uint16_t address, uint16_t count);
  void finish_scan_();
  void fail_scan_(const char *reason);
  void publish_models_text_(const std::string &text);

  uint32_t startup_delay_ms_{5000};
  uint32_t scan_retry_interval_ms_{30000};
  std::string name_prefix_{};
  std::vector<uint16_t> wanted_models_{};

  ScanState state_{ScanState::IDLE};
  bool scan_done_{false};
  uint8_t base_index_{0};
  uint8_t retries_{0};
  uint16_t pending_address_{0};
  std::vector<ModelInfo> models_{};

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *models_text_sensor_{nullptr};
#endif
};

}  // namespace esphome::sunspec_client
