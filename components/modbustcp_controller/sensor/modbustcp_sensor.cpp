
#include "modbustcp_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbustcp_controller {

static const char *const TAG = "modbustcp_controller.sensor";

void ModbusTCPSensor::dump_config() { LOG_SENSOR(TAG, "Modbus Controller Sensor", this); }

void ModbusTCPSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  float result = payload_to_float(data, *this);

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->transform_func_.has_value()) {
    // the lambda can parse the response itself
    auto val = (*this->transform_func_)(this, result, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      result = val.value();
    }
  }
  ESP_LOGD(TAG, "Sensor new state: %.02f", result);
  // this->sensor_->raw_state = result;
  this->publish_state(result);
}

}  // namespace modbustcp_controller
}  // namespace esphome
