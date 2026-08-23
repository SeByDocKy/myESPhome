#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../pcm3k6w.h"

namespace esphome::pcm3k6w {

// Plain sensor::Sensor specialisation. It carries no extra state today - the
// hub is told which SensorKind each instance corresponds to at codegen time
// (see sensor/__init__.py) and publishes straight to the pointer it was
// handed, so this class exists mainly for namespacing and future per-entity
// extension (e.g. a calibration offset).
class PCM3K6WSensor : public sensor::Sensor {};

}  // namespace esphome::pcm3k6w
