#include "bno055.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <string>

namespace esphome {
namespace bno055 {

static const char *const TAG = "bno055";

// --- Register map (Bosch BNO055 datasheet, §4.2) --------------------------
static const uint8_t REG_CHIP_ID = 0x00;
static const uint8_t CHIP_ID_VALUE = 0xA0;

static const uint8_t REG_PAGE_ID = 0x07;

static const uint8_t REG_ACCEL_DATA = 0x08;   // 6 bytes: X, Y, Z (int16 LE each)
static const uint8_t REG_MAG_DATA = 0x0E;     // 6 bytes
static const uint8_t REG_GYRO_DATA = 0x14;    // 6 bytes
static const uint8_t REG_EULER_DATA = 0x1A;   // 6 bytes: heading, roll, pitch
static const uint8_t REG_QUATERNION_DATA = 0x20;  // 8 bytes: w, x, y, z
static const uint8_t REG_LINEAR_ACCEL_DATA = 0x28;  // 6 bytes
static const uint8_t REG_GRAVITY_DATA = 0x2E;       // 6 bytes

static const uint8_t REG_TEMP = 0x34;
static const uint8_t REG_CALIB_STAT = 0x35;
static const uint8_t REG_SYS_STATUS = 0x39;
static const uint8_t REG_SYS_ERR = 0x3A;

static const uint8_t REG_UNIT_SEL = 0x3B;
static const uint8_t REG_OPR_MODE = 0x3D;
static const uint8_t REG_PWR_MODE = 0x3E;
static const uint8_t REG_SYS_TRIGGER = 0x3F;
static const uint8_t REG_AXIS_MAP_CONFIG = 0x41;
static const uint8_t REG_AXIS_MAP_SIGN = 0x42;

// Offset registers: ACCEL_OFFSET_X/Y/Z (6B), MAG_OFFSET_X/Y/Z (6B),
// GYRO_OFFSET_X/Y/Z (6B), ACCEL_RADIUS (2B), MAG_RADIUS (2B) = 22 bytes.
static const uint8_t REG_OFFSETS_START = 0x55;
static const size_t OFFSETS_LEN = 22;

static const uint8_t PWR_MODE_NORMAL = 0x00;

static const uint8_t SYS_TRIGGER_RST_SYS = 0x20;
static const uint8_t SYS_TRIGGER_CLK_SEL_EXTERNAL = 0x80;

// Fixed-point -> physical unit conversion factors (datasheet §3.6.5).
// These correspond to the default UNIT_SEL = 0x00 we write in setup():
// m/s^2, dps, degrees, °C, Windows orientation.
static const float ACCEL_LSB_PER_MS2 = 100.0f;
static const float GYRO_LSB_PER_DPS = 16.0f;
static const float EULER_LSB_PER_DEGREE = 16.0f;
static const float MAG_LSB_PER_UT = 16.0f;
static const float QUATERNION_LSB_PER_UNIT = 16384.0f;  // fixed, not affected by UNIT_SEL

namespace {
// Trivial fixed-size wrapper so we can hand a POD type to the flash
// preferences API without exposing the raw offset layout in the header.
struct BNO055CalibData {
  uint8_t bytes[OFFSETS_LEN];
};

const char *sys_status_to_string(uint8_t status) {
  switch (status) {
    case 0:
      return "Idle";
    case 1:
      return "System error";
    case 2:
      return "Initializing peripherals";
    case 3:
      return "System initialization";
    case 4:
      return "Executing self-test";
    case 5:
      return "Sensor fusion running";
    case 6:
      return "Running without fusion";
    default:
      return "Unknown";
  }
}

const char *sys_error_to_string(uint8_t error) {
  switch (error) {
    case 0:
      return "No error";
    case 1:
      return "Peripheral initialization error";
    case 2:
      return "System initialization error";
    case 3:
      return "Self-test failed";
    case 4:
      return "Register map value out of range";
    case 5:
      return "Register map address out of range";
    case 6:
      return "Register map write error";
    case 7:
      return "Low power mode not available for selected operation mode";
    case 8:
      return "Accelerometer power mode not available";
    case 9:
      return "Fusion algorithm configuration error";
    case 10:
      return "Sensor configuration error";
    default:
      return "Unknown";
  }
}
}  // namespace

void BNO055Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BNO055...");

  // One preference slot per I2C address, in case several BNO055 are used.
  this->calibration_pref_ = global_preferences->make_preference<BNO055CalibData>(
      fnv1_hash("bno055_calib_" + std::to_string(this->address_)));

  uint8_t chip_id = 0;
  if (!this->read_byte(REG_CHIP_ID, &chip_id) || chip_id != CHIP_ID_VALUE) {
    ESP_LOGE(TAG, "Chip ID mismatch (got 0x%02X, expected 0x%02X) - check wiring/address", chip_id,
              CHIP_ID_VALUE);
    this->mark_failed();
    return;
  }

  // Must be in CONFIG mode before touching PAGE_ID / UNIT_SEL / SYS_TRIGGER.
  if (!this->write_operation_mode_(OperationMode::MODE_CONFIG)) {
    this->mark_failed();
    return;
  }

  // Full chip reset so we start from a known state.
  this->write_byte(REG_SYS_TRIGGER, SYS_TRIGGER_RST_SYS);
  delay(700);  // datasheet: POR/reset time up to 650ms

  uint32_t start = millis();
  chip_id = 0;
  while (millis() - start < 1000) {
    if (this->read_byte(REG_CHIP_ID, &chip_id) && chip_id == CHIP_ID_VALUE)
      break;
    delay(10);
  }
  if (chip_id != CHIP_ID_VALUE) {
    ESP_LOGE(TAG, "BNO055 did not come back after reset");
    this->mark_failed();
    return;
  }

  this->write_byte(REG_PWR_MODE, PWR_MODE_NORMAL);
  delay(10);
  this->write_byte(REG_PAGE_ID, 0x00);

  // 0x00 == m/s^2, dps, degrees, °C, Windows orientation (all defaults).
  this->write_byte(REG_UNIT_SEL, 0x00);

  // Reset already restored AXIS_MAP_* to chip defaults (identity mapping);
  // only touch the registers if the user configured a non-default mounting.
  if (this->axis_remap_set_) {
    this->write_byte(REG_AXIS_MAP_CONFIG, this->axis_map_config_);
    this->write_byte(REG_AXIS_MAP_SIGN, this->axis_map_sign_);
  }

  // Still in CONFIG mode here, which is required to write offset registers.
  if (this->restore_calibration_) {
    BNO055CalibData data;
    if (this->calibration_pref_.load(&data)) {
      if (this->write_offsets_(data.bytes)) {
        ESP_LOGI(TAG, "Restored saved calibration offsets from flash");
      }
    } else {
      ESP_LOGD(TAG, "No saved calibration offsets found");
    }
  }

  if (this->use_external_crystal_) {
    this->write_byte(REG_SYS_TRIGGER, SYS_TRIGGER_CLK_SEL_EXTERNAL);
    delay(10);
  }

  if (!this->write_operation_mode_(this->operation_mode_)) {
    this->mark_failed();
    return;
  }

  this->initialized_ = true;
}

bool BNO055Component::write_operation_mode_(OperationMode mode) {
  if (!this->write_byte(REG_OPR_MODE, static_cast<uint8_t>(mode))) {
    ESP_LOGE(TAG, "Failed to write operation mode 0x%02X", static_cast<uint8_t>(mode));
    return false;
  }
  // CONFIG mode needs >=19ms to settle, every other mode needs >=7ms.
  delay(mode == OperationMode::MODE_CONFIG ? 19 : 7);
  return true;
}

bool BNO055Component::read_vector_(uint8_t reg, float *x, float *y, float *z, float lsb_per_unit) {
  uint8_t buf[6];
  if (!this->read_bytes(reg, buf, 6))
    return false;
  int16_t raw_x = static_cast<int16_t>((uint16_t(buf[1]) << 8) | buf[0]);
  int16_t raw_y = static_cast<int16_t>((uint16_t(buf[3]) << 8) | buf[2]);
  int16_t raw_z = static_cast<int16_t>((uint16_t(buf[5]) << 8) | buf[4]);
  *x = raw_x / lsb_per_unit;
  *y = raw_y / lsb_per_unit;
  *z = raw_z / lsb_per_unit;
  return true;
}

bool BNO055Component::read_quaternion_() {
  uint8_t buf[8];
  if (!this->read_bytes(REG_QUATERNION_DATA, buf, 8))
    return false;
  int16_t raw_w = static_cast<int16_t>((uint16_t(buf[1]) << 8) | buf[0]);
  int16_t raw_x = static_cast<int16_t>((uint16_t(buf[3]) << 8) | buf[2]);
  int16_t raw_y = static_cast<int16_t>((uint16_t(buf[5]) << 8) | buf[4]);
  int16_t raw_z = static_cast<int16_t>((uint16_t(buf[7]) << 8) | buf[6]);
  this->quaternion_w_ = raw_w / QUATERNION_LSB_PER_UNIT;
  this->quaternion_x_ = raw_x / QUATERNION_LSB_PER_UNIT;
  this->quaternion_y_ = raw_y / QUATERNION_LSB_PER_UNIT;
  this->quaternion_z_ = raw_z / QUATERNION_LSB_PER_UNIT;
  return true;
}

bool BNO055Component::read_temperature_() {
  uint8_t raw = 0;
  if (!this->read_byte(REG_TEMP, &raw))
    return false;
  this->temperature_ = static_cast<float>(static_cast<int8_t>(raw));  // 1 LSB = 1°C
  return true;
}

bool BNO055Component::read_calibration_() {
  uint8_t raw = 0;
  if (!this->read_byte(REG_CALIB_STAT, &raw))
    return false;
  this->calib_system_ = (raw >> 6) & 0x03;
  this->calib_gyro_ = (raw >> 4) & 0x03;
  this->calib_accel_ = (raw >> 2) & 0x03;
  this->calib_mag_ = raw & 0x03;
  return true;
}

bool BNO055Component::read_system_status_(uint8_t *status, uint8_t *error) {
  if (!this->read_byte(REG_SYS_STATUS, status))
    return false;
  if (!this->read_byte(REG_SYS_ERR, error))
    return false;
  return true;
}

bool BNO055Component::read_offsets_(uint8_t *buf22) {
  // Offsets can only be read back reliably in CONFIG mode (datasheet
  // §3.6.4); briefly switch there and back to the configured fusion mode.
  if (!this->write_operation_mode_(OperationMode::MODE_CONFIG))
    return false;
  bool ok = this->read_bytes(REG_OFFSETS_START, buf22, OFFSETS_LEN);
  if (!this->write_operation_mode_(this->operation_mode_))
    ok = false;
  return ok;
}

bool BNO055Component::write_offsets_(const uint8_t *buf22) {
  // Caller is expected to already be in CONFIG mode (true during setup()).
  return this->write_bytes(REG_OFFSETS_START, buf22, OFFSETS_LEN);
}

void BNO055Component::maybe_save_calibration_() {
  if (!this->auto_save_calibration_)
    return;

  bool fully_calibrated = this->calib_system_ == 3 && this->calib_gyro_ == 3 &&
                           this->calib_accel_ == 3 && this->calib_mag_ == 3;
  if (!fully_calibrated) {
    // Re-arm so a future full calibration (e.g. after the magnetometer
    // drifts and gets re-calibrated) gets saved again too.
    this->calibration_saved_ = false;
    return;
  }
  if (this->calibration_saved_)
    return;

  BNO055CalibData data;
  if (!this->read_offsets_(data.bytes)) {
    ESP_LOGW(TAG, "Fully calibrated but failed to read offsets for saving");
    return;
  }
  if (this->calibration_pref_.save(&data)) {
    ESP_LOGI(TAG, "Fully calibrated - saved calibration offsets to flash");
    this->calibration_saved_ = true;
  } else {
    ESP_LOGW(TAG, "Failed to save calibration offsets to flash");
  }
}

void BNO055Component::publish_(sensor::Sensor *sensor, float value) {
  if (sensor != nullptr)
    sensor->publish_state(value);
}

void BNO055Component::publish_(text_sensor::TextSensor *sensor, const std::string &value) {
  if (sensor != nullptr)
    sensor->publish_state(value);
}

void BNO055Component::update() {
  if (!this->initialized_ || this->is_failed())
    return;

  bool ok = true;
  ok &= this->read_vector_(REG_ACCEL_DATA, &this->accel_x_, &this->accel_y_, &this->accel_z_,
                            ACCEL_LSB_PER_MS2);
  ok &= this->read_vector_(REG_MAG_DATA, &this->mag_x_, &this->mag_y_, &this->mag_z_, MAG_LSB_PER_UT);
  ok &= this->read_vector_(REG_GYRO_DATA, &this->gyro_x_, &this->gyro_y_, &this->gyro_z_,
                            GYRO_LSB_PER_DPS);
  // Euler register order is heading, roll, pitch.
  ok &= this->read_vector_(REG_EULER_DATA, &this->heading_, &this->roll_, &this->pitch_,
                            EULER_LSB_PER_DEGREE);
  ok &= this->read_quaternion_();
  ok &= this->read_vector_(REG_LINEAR_ACCEL_DATA, &this->linear_accel_x_, &this->linear_accel_y_,
                            &this->linear_accel_z_, ACCEL_LSB_PER_MS2);
  ok &= this->read_vector_(REG_GRAVITY_DATA, &this->gravity_x_, &this->gravity_y_, &this->gravity_z_,
                            ACCEL_LSB_PER_MS2);
  ok &= this->read_temperature_();
  ok &= this->read_calibration_();
  ok &= this->read_system_status_(&this->sys_status_, &this->sys_error_);

  if (!ok) {
    ESP_LOGW(TAG, "Failed to read BNO055 data over I2C");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  this->publish_(this->heading_sensor_, this->heading_);
  this->publish_(this->roll_sensor_, this->roll_);
  this->publish_(this->pitch_sensor_, this->pitch_);

  this->publish_(this->quaternion_w_sensor_, this->quaternion_w_);
  this->publish_(this->quaternion_x_sensor_, this->quaternion_x_);
  this->publish_(this->quaternion_y_sensor_, this->quaternion_y_);
  this->publish_(this->quaternion_z_sensor_, this->quaternion_z_);

  this->publish_(this->linear_acceleration_x_sensor_, this->linear_accel_x_);
  this->publish_(this->linear_acceleration_y_sensor_, this->linear_accel_y_);
  this->publish_(this->linear_acceleration_z_sensor_, this->linear_accel_z_);

  this->publish_(this->gravity_x_sensor_, this->gravity_x_);
  this->publish_(this->gravity_y_sensor_, this->gravity_y_);
  this->publish_(this->gravity_z_sensor_, this->gravity_z_);

  this->publish_(this->acceleration_x_sensor_, this->accel_x_);
  this->publish_(this->acceleration_y_sensor_, this->accel_y_);
  this->publish_(this->acceleration_z_sensor_, this->accel_z_);

  this->publish_(this->gyroscope_x_sensor_, this->gyro_x_);
  this->publish_(this->gyroscope_y_sensor_, this->gyro_y_);
  this->publish_(this->gyroscope_z_sensor_, this->gyro_z_);

  this->publish_(this->magnetic_field_x_sensor_, this->mag_x_);
  this->publish_(this->magnetic_field_y_sensor_, this->mag_y_);
  this->publish_(this->magnetic_field_z_sensor_, this->mag_z_);

  this->publish_(this->temperature_sensor_, this->temperature_);

  this->publish_(this->calibration_system_sensor_, this->calib_system_);
  this->publish_(this->calibration_gyroscope_sensor_, this->calib_gyro_);
  this->publish_(this->calibration_accelerometer_sensor_, this->calib_accel_);
  this->publish_(this->calibration_magnetometer_sensor_, this->calib_mag_);

  this->publish_(this->system_status_text_sensor_, std::string(sys_status_to_string(this->sys_status_)));
  this->publish_(this->system_error_text_sensor_, std::string(sys_error_to_string(this->sys_error_)));
  if (this->sys_status_ == 1) {
    ESP_LOGW(TAG, "BNO055 system error: %s", sys_error_to_string(this->sys_error_));
  }

  // May briefly switch to CONFIG mode and back if this is the first update
  // where the chip reports full calibration - done last so it doesn't
  // delay the sensor data published above.
  this->maybe_save_calibration_();
}

void BNO055Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BNO055:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with BNO055 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Operation mode: 0x%02X", static_cast<uint8_t>(this->operation_mode_));
  ESP_LOGCONFIG(TAG, "  External crystal: %s", YESNO(this->use_external_crystal_));
  ESP_LOGCONFIG(TAG, "  Auto-save calibration: %s", YESNO(this->auto_save_calibration_));
  ESP_LOGCONFIG(TAG, "  Restore calibration: %s", YESNO(this->restore_calibration_));
  if (this->axis_remap_set_) {
    ESP_LOGCONFIG(TAG, "  Axis remap: config=0x%02X sign=0x%02X", this->axis_map_config_,
                  this->axis_map_sign_);
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace bno055
}  // namespace esphome
