#pragma once

/**
 * rylr998_sensor.h
 *
 * Thin sensor wrappers for the RYLR998 LoRa component.
 *
 * The actual sensor publication is performed directly by RYLR998Component
 * through pointers to standard esphome::sensor::Sensor objects.
 */

#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace rylr998 {
namespace sensor {

class RYLR998RSSISensor : public esphome::sensor::Sensor {};

class RYLR998SNRSensor : public esphome::sensor::Sensor {};


class RYLR998LastErrorSensor : public esphome::sensor::Sensor {};

}  // namespace sensor
}  // namespace rylr998
}  // namespace esphome
