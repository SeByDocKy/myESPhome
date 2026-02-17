#pragma once

/**
 * rylr998_sensor.h
 *
 * Thin sensor wrappers for the RYLR998 LoRa component.
 *
 * The actual sensor publication is performed directly by RYLR998Component
 * through pointers to standard esphome::sensor::Sensor objects.
 *
 * This header is kept for future extensibility (e.g. custom filtering,
 * on_value callbacks, or computed sensors) without modifying the parent class.
 */

#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace rylr998 {
namespace sensor {

// ── RSSI Sensor ───────────────────────────────────────────────────────────────
/**
 * Publishes the Received Signal Strength Indicator of the last LoRa packet,
 * expressed in dBm.  Typical values range from −30 dBm (very strong)
 * to −130 dBm (noise floor).
 */
class RYLR998RSSISensor : public esphome::sensor::Sensor {
 public:
  // No extra methods needed — RYLR998Component calls publish_state() directly.
};

// ── SNR Sensor ────────────────────────────────────────────────────────────────
/**
 * Publishes the Signal-to-Noise Ratio of the last LoRa packet, expressed in dB.
 * Positive values indicate a signal above the noise floor; negative values
 * indicate a signal below (LoRa can still decode down to ~−20 dB).
 */
class RYLR998SNRSensor : public esphome::sensor::Sensor {
 public:
  // No extra methods needed — RYLR998Component calls publish_state() directly.
};

}  // namespace sensor
}  // namespace rylr998
}  // namespace esphome
