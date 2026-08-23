#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../pcm3k6w.h"

namespace esphome::pcm3k6w {

// See PCM3K6WSensor: state is pushed directly by the hub, this type exists
// for namespacing / future extension.
class PCM3K6WBinarySensor : public binary_sensor::BinarySensor {};

}  // namespace esphome::pcm3k6w
