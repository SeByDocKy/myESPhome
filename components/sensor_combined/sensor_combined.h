#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace sensor_combined {

// Keep in sync with the OPERATIONS dict in sensor.py.
enum CombineOperation {
  COMBINE_OP_SUM = 0,
  COMBINE_OP_MEAN = 1,
  COMBINE_OP_PROD = 2,
};

// A sensor whose state is a combination (sum / mean / product) of one
// or more other sensors. It recomputes and republishes automatically
// every time any of its source sensors publishes a new state.
class SensorCombined : public sensor::Sensor, public Component {
 public:
  // Called once per entry in the YAML "sensors" list (see sensor.py).
  void add_sensor(sensor::Sensor *s) { this->sensors_.push_back(s); }

  // cv.enum() emits a plain int literal, hence the int parameter and
  // the internal static_cast (established pattern for this codebase).
  void set_operation(int operation) { this->operation_ = static_cast<CombineOperation>(operation); }

  // false (default): a NaN source sensor is skipped, the operation
  // runs on the remaining valid values.
  // true: any NaN source sensor makes the whole result NaN.
  void set_propagate_nan(bool propagate_nan) { this->propagate_nan_ = propagate_nan; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  // Recomputes the combined value from the current state of every
  // source sensor and publishes it. Does nothing until every source
  // sensor has published at least one state.
  void update_();

  std::vector<sensor::Sensor *> sensors_;
  CombineOperation operation_{COMBINE_OP_SUM};
  bool propagate_nan_{false};
};

}  // namespace sensor_combined
}  // namespace esphome
