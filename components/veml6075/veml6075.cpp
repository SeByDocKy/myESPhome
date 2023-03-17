#include "veml6075.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace veml6075 {

static const char *const TAG = "veml6075";

// void VEML6075Component::setcoefficients(float UVA1, float UVA2, float UVB1, float UVB1, float UVA_RESP, float UVB_RESP) {
//    uva1     = UVA1;
//    uva2     = UVA2;
//    uvb1     = UVB1;
//    uvb2     = UVB2;
//    uva_resp = UVA_RESP;
//    uvb_resp = UVB_RESP;
// }

void VEML6075Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Dump data");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "uva", this->uva_sensor_);
  LOG_SENSOR("  ", "uvb", this->uvb_sensor_);
  LOG_SENSOR("  ", "uvindex", this->uvindex_sensor_);
  LOG_SENSOR("  ", "visible compensation", this->visible_comp_sensor_);
  LOG_SENSOR("  ", "ir compesensation", this->ir_comp_sensor_);
  LOG_SENSOR("  ", "rawuva", this->rawuva_sensor_);
  LOG_SENSOR("  ", "rawuvb", this->rawuvb_sensor_);
}

void VEML6075Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VEML6075...");
  
  uint8_t chip_id = 0;
  uint8_t conf_register = 0;
  
//   setCoefficients(VEML6075_DEFAULT_UVA1_COEFF, VEML6075_DEFAULT_UVA2_COEFF,
//                   VEML6075_DEFAULT_UVB1_COEFF, VEML6075_DEFAULT_UVB2_COEFF,
//                   VEML6075_DEFAULT_UVA_RESP, VEML6075_DEFAULT_UVB_RESP);

  identifychip(); // check if it's a genuine chip
  
  shutdown(true); // Shut down to change settings

  // Set Force readings
  forcedmode(this->af_);

  // Set integration time
  integrationtime(this->it_);

  // Set high dynamic
  highdynamic(this->hd_);

  shutdown(false); // Turn on chip after setting set

}

void VEML6075Component::identifychip(void){
  uint8_t chip_id;
//  uint8_t conf_register = 0;
  
  if (!this->read_byte(VEML6075_REG_ID, &chip_id)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 to check chip ID");
    this->mark_failed();
    return;
  }
  if (chip_id != VEML6075_ID) {
    ESP_LOGE(TAG, "Wrong ID, received %d, expecting %d", chip_id , VEML6075_ID);
//     this->error_code_ = WRONG_CHIP_ID;
    this->mark_failed();
    return;
  }
  
/*  
  if (!this->read_byte(VEML6075_REG_CONF, &conf_register)) {
    ESP_LOGE(TAG, "Can't communicate with VEML6075");
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  */
  
}
 
void VEML6075Component::shutdown(bool stop){
  uint8_t conf , sd = 0;
  
  if (!this->read_byte(VEML6075_REG_CONF, &conf)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in shutdown");
    this->mark_failed();
    return;
  }
  if (stop == true){ sd = 1;}
  
  conf &= ~(VEML6075_SHUTDOWN_MASK);     // Clear shutdown bit
  conf |= sd << VEML6075_SHUTDOWN_SHIFT; //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  if (!this->write_byte(VEML6075_REG_CONF, conf)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to turn on/off chip");
     return;
  }
}
 
void VEML6075Component::forcedmode(veml6075_af_t af){
  uint8_t conf;
  if (!this->read_byte(VEML6075_REG_CONF, &conf)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in forcemode");
    this->mark_failed();
    return;
  }
  
  conf &= ~(VEML6075_AF_MASK);     // Clear shutdown bit
  conf |= (uint8_t)af << VEML6075_AF_SHIFT; //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  if (!this->write_byte(VEML6075_REG_CONF, conf)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set autoforce mode");
     return;
  }
}
  
void VEML6075Component::integrationtime(veml6075_uv_it_t it){
  uint8_t conf;
  if (!this->read_byte(VEML6075_REG_CONF, &conf)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in integration time");
    this->mark_failed();
    return;
  }
  
  conf &= ~(VEML6075_UV_IT_MASK);     // Clear shutdown bit
  conf |= (uint8_t)it << VEML6075_UV_IT_SHIFT; //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  if (!this->write_byte(VEML6075_REG_CONF, conf)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set integration time mode");
     return;
  }
  
  this->uva_responsivity_ = VEML6075_UVA_RESPONSIVITY[(uint8_t)it];
  this->uva_responsivity_ = VEML6075_UVB_RESPONSIVITY[(uint8_t)it];

  switch (it){
    case IT_50MS:
        this->integrationtime_ = 50;
        break;
    case IT_100MS:
        this->integrationtime_ = 100;
        break;
    case IT_200MS:
        this->integrationtime_ = 200;
        break;
    case IT_400MS:
        this->integrationtime_ = 400;
        break;
    case IT_800MS:
        this->integrationtime_ = 800;
        break;
    default:
        this->integrationtime_ = 0;
    } 
}

void VEML6075Component::highdynamic(veml6075_hd_t hd){
  uint8_t conf;
  
  if (!this->read_byte(VEML6075_REG_CONF, &conf)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in high dynamic");
    this->mark_failed();
    return;
  }
  if (hd == DYNAMIC_HIGH){
        this->hdenabled_ = true;
  }
  else{
        this->hdenabled_ = false;
  }
  
  conf &= ~(VEML6075_HD_MASK);     // Clear shutdown bit
  conf |= (uint8_t)hd << VEML6075_HD_SHIFT; 
  if (!this->write_byte(VEML6075_REG_CONF, conf)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set high dynamic mode");
     return;
  }
}  
 
uint16_t VEML6075Component::calc_visible_comp(void){
    uint8_t data[2] = {0, 0};
    if (!this->read_bytes(VEML6075_REG_VISIBLE_COMP, (uint8_t *) &data, 2)){
       ESP_LOGW(TAG, "can't read VEML6075_REG_VISIBLE_COMP register");
       this->mark_failed();
//       return;	  
    }
    return (data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8);
}

uint16_t VEML6075Component::calc_ir_comp(void){
    uint8_t data[2] = {0, 0};
    if (!this->read_bytes(VEML6075_REG_IR_COMP, (uint8_t *) &data, 2)){
       ESP_LOGW(TAG, "can't read VEML6075_REG_IR_COMP register");
       this->mark_failed();
//       return;	  
    }
    return (data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8);
}
		
uint16_t VEML6075Component::calc_rawuva(void){
    uint8_t data[2] = {0, 0};
    if (!this->read_bytes(VEML6075_REG_UVA , (uint8_t *) &data, 2)){
       ESP_LOGW(TAG, "can't read VEML6075_REG_UVA  register");
       this->mark_failed();
//       return;
    }
    return (data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8);	    
}
	
uint16_t VEML6075Component::calc_rawuvb(void){
    uint8_t data[2] = {0, 0};
    if (!this->read_bytes(VEML6075_REG_UVB , (uint8_t *) &data, 2)){
       ESP_LOGW(TAG, "can't read VEML6075_REG_UVB  register");
       this->mark_failed();
//       return; 
    }
    return (data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8);	    
}
	
float VEML6075Component::calc_uva(void){
    return (float)this->rawuva_ - ( ( VEML6075_DEFAULT_UVA1_COEFF * VEML6075_UV_ALPHA * this->visible_comp_) / VEML6075_UV_GAMMA ) - ( (VEML6075_UVA2_COEFF * VEML6075_UV_ALPHA * this->ir_comp_) / VEML6075_UV_DELTA );
}

float VEML6075Component::calc_uvb(void){
    return (float)this->rawuvb_ - ( ( VEML6075_DEFAULT_UVB1_COEFF * VEML6075_UV_BETA * this->visible_comp_) / VEML6075_UV_GAMMA ) - ( (VEML6075_UVB2_COEFF * VEML6075_UV_BETA * this->ir_comp_) / VEML6075_UV_DELTA );	
}	

float VEML6075Component::calc_uvindex(void){
    float index;
    float uvia = this->uva_ * (1.0 / VEML6075_UV_ALPHA) * this->uva_responsivity_;
    float uvib = this->uvb_ * (1.0 / VEML6075_UV_BETA)  * this->uvb_responsivity_;
    index = (uvia + uvib) / 2.0;
    if (this->hdenabled_ )
    {
        index *= VEML6075_HD_SCALAR;
    }
    return index;
}	
	
void VEML6075Component::update() {
  uint16_t visible_compensation , ir_compensation;
  uint16_t rawuva , rawuvb;
  float uva , uvb , index;
  
  visible_compensation  = calc_visible_comp();
  if (this->visible_comp_sensor_ != nullptr) {
	  this->visible_comp_sensor_ ->publish_state(visible_compensation);
	  ESP_LOGD(TAG, "visible_compensation: %d" , visible_compensation);
  }
  
  ir_compensation       = calc_ir_comp();
  if (this->ir_comp_sensor_ != nullptr) {
	  this->ir_comp_sensor_ ->publish_state(ir_compensation);
	  ESP_LOGD(TAG, "ir_compensation: %d" , ir_compensation);
  }
	
  rawuva                = calc_rawuva();
  if (this->rawuva_sensor_ != nullptr) {
	  this->rawuva_sensor_ ->publish_state(rawuva);
	  ESP_LOGD(TAG, "raw UVA: %d" , rawuva);
  }

  rawuvb                = calc_rawuvb();
  if (this->rawuvb_sensor_ != nullptr) {
	  this->rawuvb_sensor_ ->publish_state(rawuvb);
	  ESP_LOGD(TAG, "raw UVB: %d" , rawuvb);
  }
	
  uva                  = calc_uva();
  if (this->uva_sensor_ != nullptr) {
	  this->uva_sensor_ ->publish_state(uva);
	  ESP_LOGD(TAG, "UVA: %f" , uva);
  }
	
  uvb                  = calc_uvb();
  if (this->uva_sensor_ != nullptr) {
	  this->uvb_sensor_ ->publish_state(uvb);
	  ESP_LOGD(TAG, "UVB: %f" , uvb);
  }
	
  uvindex              = calc_uvindex();
   if (this->uvindex_sensor_ != nullptr) {
	  this->uvindex_sensor_ ->publish_state(uvindex);
	  ESP_LOGD(TAG, "UV index: %f" , uvindex);
  }
}

float VEML6075Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace veml6075
}  // namespace esphome
