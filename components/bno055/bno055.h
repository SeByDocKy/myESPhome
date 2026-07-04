#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace bno055 {

// Operation mode register values (BNO055 datasheet §3.3, table 3-5).
// is_class=True on the Python side => strongly typed enum class in C++.
enum class OperationMode : uint8_t {
  MODE_CONFIG = 0x00,
  MODE_ACCONLY = 0x01,
  MODE_MAGONLY = 0x02,
  MODE_GYROONLY = 0x03,
  MODE_ACCMAG = 0x04,
  MODE_ACCGYRO = 0x05,
  MODE_MAGGYRO = 0x06,
  MODE_AMG = 0x07,
  MODE_IMUPLUS = 0x08,
  MODE_COMPASS = 0x09,
  MODE_M4G = 0x0A,
  MODE_NDOF_FMC_OFF = 0x0B,
  MODE_NDOF = 0x0C,
};

class BNO055Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_operation_mode(OperationMode mode) { this->operation_mode_ = mode; }
  void set_use_external_crystal(bool use_external_crystal) {
    this->use_external_crystal_ = use_external_crystal;
  }
  void set_auto_save_calibration(bool auto_save_calibration) {
    this->auto_save_calibration_ = auto_save_calibration;
  }
  void set_restore_calibration(bool restore_calibration) {
    this->restore_calibration_ = restore_calibration;
  }
  // config/sign: raw AXIS_MAP_CONFIG (0x41) / AXIS_MAP_SIGN (0x42) register
  // values, pre-computed on the Python side from x/y/z_axis + x/y/z_sign.
  void set_axis_remap(uint8_t config, uint8_t sign) {
    this->axis_map_config_ = config;
    this->axis_map_sign_ = sign;
    this->axis_remap_set_ = true;
  }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  SUB_SENSOR(heading)
  SUB_SENSOR(roll)
  SUB_SENSOR(pitch)

  SUB_SENSOR(quaternion_w)
  SUB_SENSOR(quaternion_x)
  SUB_SENSOR(quaternion_y)
  SUB_SENSOR(quaternion_z)

  SUB_SENSOR(linear_acceleration_x)
  SUB_SENSOR(linear_acceleration_y)
  SUB_SENSOR(linear_acceleration_z)

  SUB_SENSOR(gravity_x)
  SUB_SENSOR(gravity_y)
  SUB_SENSOR(gravity_z)

  SUB_SENSOR(acceleration_x)
  SUB_SENSOR(acceleration_y)
  SUB_SENSOR(acceleration_z)

  SUB_SENSOR(gyroscope_x)
  SUB_SENSOR(gyroscope_y)
  SUB_SENSOR(gyroscope_z)

  SUB_SENSOR(magnetic_field_x)
  SUB_SENSOR(magnetic_field_y)
  SUB_SENSOR(magnetic_field_z)

  SUB_SENSOR(temperature)

  SUB_SENSOR(calibration_system)
  SUB_SENSOR(calibration_gyroscope)
  SUB_SENSOR(calibration_accelerometer)
  SUB_SENSOR(calibration_magnetometer)

  SUB_TEXT_SENSOR(system_status)
  SUB_TEXT_SENSOR(system_error)

 protected:
  // Switches the sensor to CONFIG mode, writes SYS_TRIGGER / OPR_MODE, etc.
  // CONFIG mode is required before touching PAGE_ID, UNIT_SEL, SYS_TRIGGER,
  // AXIS_MAP_* or the offset registers; every fusion/operation mode needs
  // >= 7ms to settle, CONFIG mode needs >= 19ms (datasheet table 3-6).
  bool write_operation_mode_(OperationMode mode);

  // Reads 3 consecutive 16-bit little-endian signed registers starting at
  // `reg` and converts them to physical units using `lsb_per_unit`.
  bool read_vector_(uint8_t reg, float *x, float *y, float *z, float lsb_per_unit);
  bool read_quaternion_();
  bool read_temperature_();
  bool read_calibration_();
  bool read_system_status_(uint8_t *status, uint8_t *error);

  // Offset registers (0x55-0x6A, 22 bytes: accel/mag/gyro XYZ + 2 radii).
  // Per datasheet §3.6.4 they can only be read back reliably / written in
  // CONFIG mode, so read_offsets_() briefly switches mode and switches back.
  bool read_offsets_(uint8_t *buf22);
  bool write_offsets_(const uint8_t *buf22);
  void maybe_save_calibration_();

  void publish_(sensor::Sensor *sensor, float value);
  void publish_(text_sensor::TextSensor *sensor, const std::string &value);

  OperationMode operation_mode_{OperationMode::MODE_NDOF};
  bool use_external_crystal_{true};
  bool initialized_{false};

  bool auto_save_calibration_{true};
  bool restore_calibration_{true};
  bool calibration_saved_{false};
  ESPPreferenceObject calibration_pref_;

  bool axis_remap_set_{false};
  uint8_t axis_map_config_{0x24};  // chip power-on default: identity mapping
  uint8_t axis_map_sign_{0x00};    // chip power-on default: all positive

  // Latest decoded values, refreshed once per update() cycle.
  float heading_{NAN}, roll_{NAN}, pitch_{NAN};
  float quaternion_w_{NAN}, quaternion_x_{NAN}, quaternion_y_{NAN}, quaternion_z_{NAN};
  float linear_accel_x_{NAN}, linear_accel_y_{NAN}, linear_accel_z_{NAN};
  float gravity_x_{NAN}, gravity_y_{NAN}, gravity_z_{NAN};
  float accel_x_{NAN}, accel_y_{NAN}, accel_z_{NAN};
  float gyro_x_{NAN}, gyro_y_{NAN}, gyro_z_{NAN};
  float mag_x_{NAN}, mag_y_{NAN}, mag_z_{NAN};
  float temperature_{NAN};
  uint8_t calib_system_{0}, calib_gyro_{0}, calib_accel_{0}, calib_mag_{0};
  uint8_t sys_status_{0}, sys_error_{0};
};

}  // namespace bno055
}  // namespace esphome

