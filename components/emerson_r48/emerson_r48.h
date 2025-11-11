#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace emerson_r48 {

class EmersonR48MaxCurrentOutput;

class EmersonR48Component : public PollingComponent {


 public:
  EmersonR48Component(canbus::Canbus *canbus);

  void setup() override;
  void update() override;

  void set_output_voltage(float value, bool offline = false);
  void set_max_output_current(float value, bool offline = false);
  void set_max_input_current(float value);
  void set_offline_values();

  void set_input_voltage_sensor(sensor::Sensor *input_voltage_sensor) { input_voltage_sensor_ = input_voltage_sensor; }
  void set_input_frequency_sensor(sensor::Sensor *input_frequency_sensor) {
    input_frequency_sensor_ = input_frequency_sensor;
  }
  void set_input_current_sensor(sensor::Sensor *input_current_sensor) { input_current_sensor_ = input_current_sensor; }
  void set_input_power_sensor(sensor::Sensor *input_power_sensor) { input_power_sensor_ = input_power_sensor; }
  void set_input_temp_sensor(sensor::Sensor *input_temp_sensor) { input_temp_sensor_ = input_temp_sensor; }
  void set_efficiency_sensor(sensor::Sensor *efficiency_sensor) { efficiency_sensor_ = efficiency_sensor; }
  void set_output_voltage_sensor(sensor::Sensor *output_voltage_sensor) {
    output_voltage_sensor_ = output_voltage_sensor;
  }
  void set_output_current_sensor(sensor::Sensor *output_current_sensor) {
    output_current_sensor_ = output_current_sensor;
  }
  void set_max_output_current_sensor(sensor::Sensor *max_output_current_sensor) {
     max_output_current_sensor_ = max_output_current_sensor;
  }

  void set_output_power_sensor(sensor::Sensor *output_power_sensor) { output_power_sensor_ = output_power_sensor; }
  void set_output_temp_sensor(sensor::Sensor *output_temp_sensor) { output_temp_sensor_ = output_temp_sensor; }

  void set_output_voltage_number(number::Number *output_voltage_number) {this->output_voltage_number_ = output_voltage_number;}
  void get_output_voltage_number(number::Number *output_voltage_number) {return this->output_voltage_number_;}
  void set_max_output_current_number(number::Number *max_output_current_number) {this->max_output_current_number_ = max_output_current_number;}
  void get_max_output_current_number(number::Number *max_output_current_number) {return this->max_output_current_number_;}
  void set_max_input_current_number(number::Number *max_input_current_number) {this->max_input_current_number_ = max_input_current_number;}
  void get_max_input_current_number(number::Number *max_input_current_number) {return this->max_input_current_number_;}

  void set_control(uint8_t msgv);

  void sendSync();
  void sendSync2();
  void gimme5();

  uint32_t lastCtlSent_;

  bool dcOff_ = 0;
  bool fanFull_ = 0;
  bool flashLed_ = 0;
  bool acOff_ = 0;

 protected:
  canbus::Canbus *canbus;
  uint32_t lastUpdate_;

  sensor::Sensor *input_voltage_sensor_{nullptr};
  sensor::Sensor *input_frequency_sensor_{nullptr};
  sensor::Sensor *input_current_sensor_{nullptr};
  sensor::Sensor *input_power_sensor_{nullptr};
  sensor::Sensor *input_temp_sensor_{nullptr};
  sensor::Sensor *efficiency_sensor_{nullptr};
  sensor::Sensor *output_voltage_sensor_{nullptr};
  sensor::Sensor *output_current_sensor_{nullptr};
  sensor::Sensor *max_output_current_sensor_{nullptr};
  sensor::Sensor *output_power_sensor_{nullptr};
  sensor::Sensor *output_temp_sensor_{nullptr};

  number::Number *output_voltage_number_{nullptr};
  number::Number *max_output_current_number_{nullptr};
  number::Number *max_input_current_number_{nullptr};

  EmersonR48MaxCurrentOutput *max_current_output_ = nullptr;

  // void on_frame(uint32_t can_id, bool rtr, std::vector<uint8_t> &data);
  void on_frame(uint32_t can_id, bool rtr, const std::vector<uint8_t> &data);

  void publish_sensor_state_(sensor::Sensor *sensor, float value);
  void publish_number_state_(number::Number *number, float value);
};

}  // namespace emerson_r48
}  // namespace esphome

/*

# CAN stuff
ARBITRATION_ID = 0x0607FF83 # or 06080783 ?
ARBITRATION_ID_READ = 0x06000783
BITRATE = 125000

# individual properties to read out, data: 0x01, 0xF0, 0x00, p, 0x00, 0x00, 0x00, 0x00 with p:
# 01 : output voltage
# 02 : output current
# 03 : output current limit
# 04 : temperature in C
# 05 : supply voltage
READ_COMMANDS = [0x01, 0x02, 0x03, 0x04, 0x05]

# Reads all of the above and a few more at once
READ_ALL = [0x00, 0xF0, 0x00, 0x80, 0x46, 0xA5, 0x34, 0x00] 

# 62.5A is the nominal current of Emerson/Vertiv R48-3000e and corresponds to 121%
OUTPUT_CURRENT_RATED_VALUE = 62.5
OUTPUT_CURRENT_RATED_PERCENTAGE_MIN = 10
OUTPUT_CURRENT_RATED_PERCENTAGE_MAX = 121
OUTPUT_CURRENT_RATED_PERCENTAGE = 121
OUTPUT_VOLTAGE_MIN = 41.0
OUTPUT_VOLTAGE_MAX = 58.5
OUTPUT_CURRENT_MIN = 5.5 # 10%, rounded up to nearest 0.5V 
OUTPUT_CURRENT_MAX = OUTPUT_CURRENT_RATED_VALUE

*/
