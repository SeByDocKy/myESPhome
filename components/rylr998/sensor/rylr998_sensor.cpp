#include "rylr998_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rylr998 {
namespace sensor {

static const char *const TAG = "rylr998.sensor";

/**
 * rylr998_sensor.cpp
 *
 * Both RYLR998RSSISensor and RYLR998SNRSensor are currently pure wrappers
 * around esphome::sensor::Sensor and have no additional runtime logic.
 *
 * Publication is done by RYLR998Component::process_line_() in rylr998.cpp
 * whenever a +RCV frame is received from the module:
 *
 *   if (rssi_sensor_ != nullptr) rssi_sensor_->publish_state(rssi);
 *   if (snr_sensor_  != nullptr) snr_sensor_->publish_state(snr);
 *
 * This file intentionally contains no business logic so that future
 * developers can add e.g. averaging filters or custom on_value callbacks
 * here without touching the main component.
 */

}  // namespace sensor
}  // namespace rylr998
}  // namespace esphome
