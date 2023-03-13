#include "pmwcs3.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pmwcs3 {

static const char *const TAG = "pmwcs3";
	
void PMWCS3Component::setup() {
   ESP_LOGCONFIG(TAG, "Setting up PMWCS3...");
   
/*   
   this->write_mode_register(this->mode_);
   this->write_enableid_register(this->enableid_);
   this->write_singleshot_register(this->singleshot_);
   this->write_labelnext_register(this->labelnext_);	
   this->write_persistid_register(this->persistid_);
   this->write_eraseid_register(this->eraseid_);
   this->write_debug_register(this->debug_);	
*/  
	
}
	
void PMWCS3Component::update() { this->read_data_(); }

float PMWCS3Component::get_setup_priority() const { return setup_priority::DATA; }
	

/*		
void PMWCS3Component::write_register(uint8_t reg, uint8_t bits, uint8_t start_pos) {
  uint8_t write_reg = bits;	
  write_reg |= (bits << start_pos);
  if (!this->write_byte(reg, write_reg)) {
    ESP_LOGW(TAG, "write_byte failed - increase log level for more details!");
    return;
  }
}
	
uint8_t PMWCS3Component::read_register(uint8_t reg ) {
  uint8_t value = 0;
  if (!this->read_bytes(reg , &value, sizeof(value)) ) { 	  
    ESP_LOGW(TAG, "Reading register failed!");
    return 0;
  }
  return value;  
}
*/

	
	
void PMWCS3Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PMWCS3");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PMWCS3 failed!");
  }
  ESP_LOGI(TAG, "%s", this->is_failed() ? "FAILED" : "OK");
	
  LOG_UPDATE_INTERVAL(this);
	
  LOG_SENSOR("  ", "e25", this->e25_sensor_);	
  LOG_SENSOR("  ", "ec", this->ec_sensor_);
  LOG_SENSOR("  ", "temperatue", this->temperature_sensor_);
  LOG_SENSOR("  ", "vwc", this->vwc_sensor_);
}

void PMWCS3Component::read_data_() {
  uint8_t data[2];
  uint16_t result;
//  uint16_t results[1];
  float e25, ec, temperature, vwc;
// uint16_t results;	
/*	
  if (!this->write_byte(PMWCS3_REG_READ_START, 0x01)) {
    ESP_LOGE(TAG, "Starting measurement failed!");
    this->status_set_warning();
    return;
  }	
*/	
	
	
 //  this->read_bytes_16(PMWCS3_REG_READ_E25, (uint16_t *) &results, 1);
//  this->read_bytes_16(PMWCS3_I2C_ADDRESS, (uint16_t *) &results, 4);
  this->read_bytes(PMWCS3_REG_READ_E25, (uint8_t *) &data, 2);
  result = encode_uint16(data[1], data[0]);
   if (this->e25_sensor_ != nullptr) {
	  e25 = result/100.0;
	  //e25 = results/100.0;
	  this->e25_sensor_->publish_state(e25);
//	  ESP_LOGD(TAG, "e25 = %f", e25);
	  ESP_LOGD(TAG, "e25: data[0]=%d, data[1]=%d, result=%d", data[0] , data[1],result);
//	  ESP_LOGD(TAG, "e25: data[0]=%d", results[0]);
  }
  
  this->read_bytes(PMWCS3_REG_READ_EC, (uint8_t *) &data, 2);
  result = encode_uint16(data[1], data[0]);
  //this->read_bytes_16(PMWCS3_REG_READ_EC, (uint16_t *) &results, 1);	
  if (this->ec_sensor_ != nullptr) {
//	  ec = results[1]/10.0;
	  ec = result/10.0;
	  this->ec_sensor_->publish_state(ec);
//	  ESP_LOGD(TAG, "ec = %f", ec);
//	  ESP_LOGD(TAG, "ec: data[0]=%d", results[0]);
	  ESP_LOGD(TAG, "ec: data[0]=%d, data[1]=%d, result=%d", data[0] , data[1],result);
  }
  
  this->read_bytes(PMWCS3_REG_READ_TEMP, (uint8_t *) &data, 2);
  result = encode_uint16(data[1], data[0]);	
//  this->read_bytes_16(PMWCS3_REG_READ_TEMP, (uint16_t *) &results, 1);	
  if (this->temperature_sensor_ != nullptr) {
	 // temperature = results[2]/100.0;
	  temperature = result/100.0;
	  this->temperature_sensor_->publish_state(temperature);
	 // ESP_LOGD(TAG, "temperature = %f", temperature);
	 // ESP_LOGD(TAG, "temp: data[0] = %d, data[1] = %d ", results[0] , results[1]);
	 // ESP_LOGD(TAG, "temp: data[0]=%d", results[0]);
	  ESP_LOGD(TAG, "temp: data[0]=%d, data[1]=%d, result=%d", data[0] , data[1],result); 
  }

  //this->read_bytes_16(PMWCS3_REG_READ_VWC, (uint16_t *) &results, 1);
  this->read_bytes(PMWCS3_REG_READ_VWC, (uint8_t *) &data, 2);
  result = encode_uint16(data[1], data[0]);	
  if (this->vwc_sensor_ != nullptr) {
//	  vwc = results[3]/10.0;
	  vwc = result/10.0;
	  this->vwc_sensor_->publish_state(vwc);
//	  ESP_LOGD(TAG, "vwc = %f", vwc);
//	  ESP_LOGD(TAG, "vwc: data[0]=%d", results[0]);
	  ESP_LOGD(TAG, "vwc: data[0]=%d, data[1]=%d, result=%d", data[0] , data[1],result);
  }
  
}

}  // namespace pmwcs3
}  // namespace esphome
