#pragma once

#include <deque>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include "openfirenet_protocol.h"

namespace esphome {
namespace openfirenet {

// NOTE: these enums are mirrored by the dictionaries in the platform __init__.py files.
// Keep both sides in sync.
enum SensorKey : uint8_t {
  SENSOR_ROOM_TEMPERATURE = 0,
  SENSOR_TARGET_TEMPERATURE,
  SENSOR_HEATING_POWER,
  SENSOR_FLAME_TEMPERATURE,
  SENSOR_ID_FAN_MEASURED,
  SENSOR_ID_FAN_SET,
  SENSOR_BOARD_TEMPERATURE,
  SENSOR_RSSI,
  SENSOR_PELLET_HOURS,
  SENSOR_PELLETS_TOTAL,
  SENSOR_SERVICE_COUNTDOWN,
  SENSOR_IGNITION_COUNT,
  SENSOR_ON_OFF_CYCLES,
  SENSOR_ERROR_CODE,
  SENSOR_COUNT,
};

enum TextSensorKey : uint8_t {
  TEXT_SENSOR_STATE = 0,
  TEXT_SENSOR_ERROR,
  TEXT_SENSOR_MODEL,
  TEXT_SENSOR_FIRMWARE,
  TEXT_SENSOR_MODE,
  TEXT_SENSOR_COUNT,
};

enum BinarySensorKey : uint8_t {
  BINARY_SENSOR_CONNECTED = 0,
  BINARY_SENSOR_BURNING,
  BINARY_SENSOR_ERROR,
  BINARY_SENSOR_HOPPER_LID_OPEN,
  BINARY_SENSOR_COUNT,
};

enum class Handshake : uint8_t {
  WAITING,    // nothing heard from the stove (or link lost)
  VERSION_SENT,  // version frame sent, waiting for GET_CDCDEVICE_VERSION_FINISHED
  READY,      // handshake done, polling cycle active
};

class OpenFireNetComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_frame_gap(uint32_t ms) { this->frame_gap_ms_ = ms; }

#ifdef USE_SENSOR
  void set_sensor(SensorKey key, sensor::Sensor *s) { this->sensors_[key] = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_text_sensor(TextSensorKey key, text_sensor::TextSensor *s) { this->text_sensors_[key] = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_binary_sensor(BinarySensorKey key, binary_sensor::BinarySensor *s) { this->binary_sensors_[key] = s; }
#endif
#ifdef USE_SWITCH
  void set_power_switch(switch_::Switch *s) { this->power_switch_ = s; }
#endif
#ifdef USE_NUMBER
  void set_target_temperature_number(number::Number *n) { this->target_temperature_number_ = n; }
  void set_heating_power_number(number::Number *n) { this->heating_power_number_ = n; }
#endif
#ifdef USE_SELECT
  void set_mode_select(select::Select *s) { this->mode_select_ = s; }
#endif

  // Control API used by the switch / number / select platforms. Return false when the write
  // cannot be done now (link not ready or controls not yet read).
  bool set_power(bool on);
  bool set_target_temperature(float celsius);
  bool set_heating_power(float percent);
  bool set_mode(long mode);

  bool is_ready() const { return this->handshake_ == Handshake::READY; }

 protected:
  // --- RX ---
  void rx_poll_();
  void process_frame_(const std::string &frame);
  void handle_command_(const Command &cmd);
  void on_probe_();
  void on_version_finished_();
  void on_stove_status_(const std::string &segment);
  void on_controls_(const std::string &segment);
  void on_sensors_(const std::string &segment);

  // --- TX ---
  void tx_poll_();
  void enqueue_(std::string frame);
  void enqueue_cycle_();
  bool write_control_(ControlField field, long value);

  // --- publishing ---
  void publish_controls_();
  void publish_sensors_();
  void publish_connected_(bool connected);
#ifdef USE_SENSOR
  void publish_sensor_(SensorKey key, float value);
#endif
#ifdef USE_TEXT_SENSOR
  void publish_text_(TextSensorKey key, const std::string &value);
#endif
#ifdef USE_BINARY_SENSOR
  void publish_binary_(BinarySensorKey key, bool value);
#endif

  uint32_t frame_gap_ms_{100};

  // RX framing
  std::string rx_buf_;
  uint32_t last_rx_ms_{0};
  uint32_t last_stove_activity_ms_{0};
  static constexpr uint32_t RX_SILENCE_MS = 8;          // end of a frame
  static constexpr uint32_t RX_MAX_FRAME = 4096;        // safety cap
  static constexpr uint32_t LINK_TIMEOUT_MS = 30000;    // no stove traffic -> link lost

  // TX engine
  std::deque<std::string> tx_queue_;
  std::string tx_current_;
  size_t tx_offset_{0};
  uint32_t tx_next_ms_{0};
  static constexpr uint32_t CHUNK_SPACING_MS = 2;
  static constexpr size_t MAX_TX_QUEUE = 24;

  Handshake handshake_{Handshake::WAITING};
  StatusFields status_;
  bool status_seen_{false};
  uint32_t last_version_ms_{0};

  // Last values from the stove
  std::vector<long> controls_;      // NUM_CONTROLS, valid when controls_valid_
  std::vector<long> slots_;
  bool controls_valid_{false};
  bool slots_valid_{false};

  // Optimistic hold: after a write the stove echoes the old values for a while.
  std::vector<long> pending_controls_;
  uint32_t write_hold_until_ms_{0};
  bool write_dirty_{false};
  bool hold_active_{false};
  static constexpr uint32_t WRITE_HOLD_MS = 8000;

  bool connected_published_{false};
  bool connected_{false};
  HighFrequencyLoopRequester high_freq_;

#ifdef USE_SENSOR
  sensor::Sensor *sensors_[SENSOR_COUNT]{};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *text_sensors_[TEXT_SENSOR_COUNT]{};
  std::string text_cache_[TEXT_SENSOR_COUNT];
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *binary_sensors_[BINARY_SENSOR_COUNT]{};
#endif
#ifdef USE_SWITCH
  switch_::Switch *power_switch_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *target_temperature_number_{nullptr};
  number::Number *heating_power_number_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *mode_select_{nullptr};
#endif
};

}  // namespace openfirenet
}  // namespace esphome
