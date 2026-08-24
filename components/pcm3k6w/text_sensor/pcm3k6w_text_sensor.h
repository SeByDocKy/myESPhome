#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "../pcm3k6w.h"

namespace esphome::pcm3k6w {

// Like PCM3K6WSensor: state is pushed directly by the hub (from the SN/
// version CAN responses, or derived from running_mode / offgrid_frequency),
// this type exists for namespacing / future extension.
class PCM3K6WTextSensor : public text_sensor::TextSensor {};

}  // namespace esphome::pcm3k6w
