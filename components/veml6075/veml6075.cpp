#include "veml6075.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace veml6075 {

static const char *const TAG = "veml6075";
static const uint8_t VEML6075_DEFAULT_I2C_ADRESS = 0x10;


void VEML6075Component::setCoefficients(float UV_A_1, float UV_A_2, float UV_B_1,
                                        float UV_B_1, float UV_A_response,
                                        float UV_B_response) 
{
  _uv_a_1 = UV_A_1;
  _uv_a_1 = UV_A_2;
  _uv_b_1 = UV_B_1;
  _uv_b_2 = UV_B_2;
  _uv_a_resp = UV_A_response;
  _uv_b_resp = UV_B_response;
}


void VEML6075Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HYTVEML6075:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "uv_a", this->uv_a_);
  LOG_SENSOR("  ", "uv_b", this->uv_b_);
  LOG_SENSOR("  ", "uv_index", this->uv_index_);
}

void VEML6075Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VEML6075...");
  
  uint8_t chip_id = 0;
  uint8_t conf_register = 0;
  
  setCoefficients(VEML6075_DEFAULT_UV_A_1_COEFF, VEML6075_DEFAULT_UV_A_2_COEFF,
                  VEML6075_DEFAULT_UV_B_1_COEFF, VEML6075_DEFAULT_UV_B_2_COEFF,
                  VEML6075_DEFAULT_UV_A_RESPONSE, VEML6075_DEFAULT_UV_B_RESPONSE);

  _commandRegister.reg = 0;
  
  
  // Mark as not failed before initializing. Some devices will turn off sensors to save on batteries
  // and when they come back on, the COMPONENT_STATE_FAILED bit must be unset on the component.
//  this->component_state_ &= ~COMPONENT_STATE_FAILED;

  if (!this->read_byte(VEML6075_REG_ID, &chip_id)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  if (chip_id != VEML6075_ID) {
    this->error_code_ = WRONG_CHIP_ID;
    this->mark_failed();
    return;
  }
  
  if (!this->read_byte(VEML6075_REG_CONF, &conf_register)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  
  _commandRegister.reg = 0;
  
  shutdown(true); // Shut down to change settings

  // Force readings
  setForcedMode(forcedReads);

  // Set integration time
  setIntegrationTime(itime);

  // Set high dynamic
  setHighDynamic(highDynamic);

  shutdown(false); // Re-enable

 

}

void VEML6075Component::update() {
  uint8_t raw_data[4];

  if (this->write(&raw_data[0], 0) != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Communication with VEML6075 failed! => Ask new values");
    return;
  }
  delay(50);  // NOLINT

  if (this->read(raw_data, 4) != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Communication with VEML6075 failed! => Read values");
    return;
  }
  uint16_t raw_temperature = ((raw_data[2] << 8) | raw_data[3]) >> 2;
  uint16_t raw_humidity = ((raw_data[0] & 0x3F) << 8) | raw_data[1];

  float temperature = ((float(raw_temperature)) * (165.0f / 16383.0f)) - 40.0f;
  float humidity = (float(raw_humidity)) * (100.0f / 16383.0f);

  ESP_LOGD(TAG, "Got UV_A=%.2f uW/cm² UV_B=%.2f uW/cm²  UV_INDEX = %.1f ", uv_a, uv_b , uv_index);

  if (this->uv_a_ != nullptr)
    this->uv_a_->publish_state(uv_a);
  if (this->uv_b_ != nullptr)
    this->uv_b_->publish_state(uv_b_);
  if (this->uv_index_ != nullptr)
    this->uv_index_->publish_state(uv_index_);
  this->status_clear_warning();
}
float VEML6075Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace veml6075
}  // namespace esphome