#include "veml6075.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

// built from https://github.com/sparkfun/SparkFun_VEML6075_Arduino_Library

namespace esphome {
namespace veml6075 {

static const char *const TAG = "veml6075";

float VEML6075Component::get_setup_priority() const { return setup_priority::DATA; } 
	
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
  delay(100);	
  ESP_LOGCONFIG(TAG, "Setting up VEML6075...");
  ESP_LOGD(TAG, "Setting up VEML6075...");
  
  
//  identifychip(); // check if it's a genuine chip
  	
  shutdown(true); // Shut down to change settings   VEML6075_REG_CONF(0x00) bit0-MSB/bit8 16 bit

  /*
  // Set Force readings
  forcedmode(this->af_); // autoforce/enable trigger  VEML6075_REG_CONF(0x00) bit1-MSB/bit9 16 bit
	

  // Set trigger mode
  trigger(this->trig_); // trigger mode  VEML6075_REG_CONF(0x00) bit2-MSB/bit10 16 bit
	
  // Set high dynamic
  highdynamic(this->hd_); // high dynamic  VEML6075_REG_CONF(0x00) bit3-MSB/bit11 16 bit	
  
  // Set integration time
  integrationtime(this->it_); // integration time  VEML6075_REG_CONF(0x00) bits 6-5-4-MSB/bits 14-13-12 16 bit

 */
  shutdown(false); // Turn on chip after settings set
 
}

void VEML6075Component::update() { 
	ESP_LOGD(TAG, "in update() for VEML6075...");
	this->read_data_(); 
}	

void VEML6075Component::identifychip(void){
  uint8_t data[2] = {0,0};
  uint16_t conf;
  
  if ( !this->read_bytes(VEML6075_REG_ID, (uint8_t *) &data , VEML6075_REG_SIZE) ) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 to check chip ID");
//    this->mark_failed();
    return;
  }
  
  if (data[0] != VEML6075_ID) {
    ESP_LOGE(TAG, "Wrong ID, received %d, expecting %d", data[0] , VEML6075_ID);
//     this->error_code_ = WRONG_CHIP_ID;
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Chip identification successfull, received %d, expecting %d", data[0] , VEML6075_ID);
  
  /*  
  if ( !this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE ) ) {
    ESP_LOGE(TAG, "Can't communicate with VEML6075");
 //   this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  conf  = ((data[0]) | (data[1] << 8));	
  ESP_LOGD(TAG, "Read successuffuly VEML6075_REG_CONF returning %d" , conf);
   */
  
}
 
void VEML6075Component::shutdown(bool stop){
  uint8_t data[2] = {0,0};
  uint16_t conf , sd = 0;
  
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in shutdown");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "read before masking shutdown %d %d" , data[1] , data[0]);	
	
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));	
  ESP_LOGD(TAG, "read VEML6075_REG_CONF: %d" , conf);
  
  if (stop == true){ sd = (uint16_t)1;}
  ESP_LOGD(TAG, "sd value: %d" , sd);
  ESP_LOGD(TAG, "VEML6075_SHUTDOWN_MASK: %d" ,VEML6075_SHUTDOWN_MASK);
  
  conf &= ~(VEML6075_SHUTDOWN_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_SHUTDOWN_MASK): %d" , conf);	
 
  conf |= sd << VEML6075_SHUTDOWN_SHIFT; //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
	
  ESP_LOGD(TAG, "set new VEML6075_REG_CONF to: %d" , conf);
  
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGD(TAG, "write after masking shutdown %d %d" , data[1] , data[0]);	

  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE )) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to turn on/off chip");
     return;
  }
  ESP_LOGD(TAG, "write_byte with VEML6075_REG_CONF successfull to turn on/off chip");
  delay(100);
  data[0] = 0;
  data[1] = 0;
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in shutdown");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Re read after writing %d %d" , data[1] , data[0]);
}
 
void VEML6075Component::forcedmode(veml6075_af_t af){
  uint8_t data[2]= {0,0};
  uint16_t conf;
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in forcemode");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "read before masking force mode %d %d" , data[1] , data[0]);
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));
  ESP_LOGD(TAG, "read VEML6075_REG_CONF: %d" , conf);
  conf &= ~(VEML6075_AF_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_AF_MASK): %d" , conf);
  conf |= af << VEML6075_AF_SHIFT; //VEML6075_MASK(conf, VEML6075_AF_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, "set new VEML6075_REG_CONF to: %d" , conf);
	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8); 	
  ESP_LOGD(TAG, "Wil write VEML6075_REG_CONF after masking force mode with: %d %d" , data[1] , data[0]);
	
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set autoforce mode");
     return;
  }
  ESP_LOGD(TAG, "write_bytes with VEML6075_REG_CONF successfull to set autoforce mode");
}

void VEML6075Component::trigger(veml6075_uv_trig_t trig) {
  uint8_t data[2]= {0,0};
  uint16_t conf;
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in trigger mode");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "read before masking trigger %d %d" , data[1] , data[0]);
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));
  conf &= ~(VEML6075_TRIG_MASK);     // Clear shutdown bit
  conf |= trig << VEML6075_TRIG_SHIFT; //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGD(TAG, "Wil write VEML6075_REG_CONF after masking trigger  with: %d %d" , data[1] , data[0]);
	
  ESP_LOGD(TAG, "Wil write VEML6075_REG_CONF with: %d %d" , data[1] , data[0]);
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set trigger mode");
     return;
  }
  ESP_LOGD(TAG, "write_bytes with VEML6075_REG_CONF successfull to set trigger mode");
	
	
}
  
void VEML6075Component::integrationtime(veml6075_uv_it_t it){
  uint8_t data[2]= {0,0};
  uint16_t conf;
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in integration time");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "read before masking integration time %d %d" , data[1] , data[0]);
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));
  conf &= ~(VEML6075_UV_IT_MASK);     // Clear shutdown bit
  conf |= it << VEML6075_UV_IT_SHIFT; //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  
  ESP_LOGD(TAG, "Wil write VEML6075_REG_CONF after masking integration time  with: %d %d" , data[1] , data[0]);
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set integration time mode");
     return;
  }
  ESP_LOGD(TAG, "write_bytes with VEML6075_REG_CONF successfull to set integration time mode");
  
  this->uva_responsivity_ = VEML6075_UVA_RESPONSIVITY[(uint8_t)it];
  this->uvb_responsivity_ = VEML6075_UVB_RESPONSIVITY[(uint8_t)it];

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
  uint8_t data[2]= {0,0};
  uint16_t conf;
  
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
//     this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in high dynamic");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "read before masking high dynamic %d %d" , data[1] , data[0]);
	
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));
  if (hd == DYNAMIC_HIGH){
        this->hdenabled_ = true;
  }
  else{
        this->hdenabled_ = false;
  }
  
  conf &= ~(VEML6075_HD_MASK);     // Clear shutdown bit
  conf |= hd << VEML6075_HD_SHIFT; 
	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGD(TAG, "Wil write VEML6075_REG_CONF after masking high dynamic with: %d %d" , data[1] , data[0]);
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set high dynamic mode");
     return;
  }
  ESP_LOGD(TAG, "write_byte with VEML6075_REG_CONF successfull to set high dynamic mode");
}  
 
uint16_t VEML6075Component::calc_visible_comp(void){
    uint8_t data[2] = {0,0};
    uint16_t result;
    if (!this->read_bytes(VEML6075_REG_VISIBLE_COMP, (uint8_t *) &data, VEML6075_REG_SIZE)){
       ESP_LOGE(TAG, "can't read VEML6075_REG_VISIBLE_COMP register");
       this->mark_failed();
//       return;	  
    }
    result = (data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8); 
    ESP_LOGD(TAG , "calc_visible_comp data[0]: %d, data[1]: %d, result: %d" , data[0] , data[1] , result); 
    return result;
}

uint16_t VEML6075Component::calc_ir_comp(void){
    uint8_t data[2] = {0, 0};
    uint16_t result;
    if (!this->read_bytes(VEML6075_REG_IR_COMP, (uint8_t *) &data, VEML6075_REG_SIZE)){
       ESP_LOGE(TAG, "can't read VEML6075_REG_IR_COMP register");
       this->mark_failed();
//       return;	  
    }
    result = (data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8); 
    ESP_LOGD(TAG , "calc_ir_comp data[0]: %d, data[1]: %d, result: %d" , data[0] , data[1] , result); 
    return result;
}
		
uint16_t VEML6075Component::calc_rawuva(void){
    uint8_t data[2] = {0, 0};
    uint16_t result;
    if (!this->read_bytes(VEML6075_REG_UVA , (uint8_t *) &data, VEML6075_REG_SIZE)){
       ESP_LOGE(TAG, "can't read VEML6075_REG_UVA  register");
       this->mark_failed();
//       return;
    }
    result = (data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8); 
    ESP_LOGD(TAG , "calc_rawuva data[0]: %d, data[1]: %d, result: %d" , data[0] , data[1] , result); 
    return result;	    
}
	
uint16_t VEML6075Component::calc_rawuvb(void){
    uint8_t data[2] = {0, 0};
    uint16_t result;
    if (!this->read_bytes(VEML6075_REG_UVB , (uint8_t *) &data, VEML6075_REG_SIZE)){
       ESP_LOGE(TAG, "can't read VEML6075_REG_UVB  register");
       this->mark_failed();
//       return; 
    }
    result = (data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8);
    ESP_LOGD(TAG , "calc_rawuvb data[0]: %d, data[1]: %d, result: %d" , data[0] , data[1] , result); 	
    return result;	    
}
	
float VEML6075Component::calc_uva(void){
    uint16_t rawuva = (uint16_t)this->rawuva_sensor_->get_state();
    uint16_t visible_comp =  (uint16_t)this->visible_comp_sensor_->get_state(); 
    uint16_t ir_comp = (uint16_t)this->ir_comp_sensor_->get_state(); 
    return (float)(rawuva) - ( ( VEML6075_UVA1_COEFF * VEML6075_UV_ALPHA * visible_comp) / VEML6075_UV_GAMMA ) - ( (VEML6075_UVA2_COEFF * VEML6075_UV_ALPHA * ir_comp) / VEML6075_UV_DELTA );
}

float VEML6075Component::calc_uvb(void){
    uint16_t rawuvb = (uint16_t)this->rawuvb_sensor_->get_state();
    uint16_t visible_comp =  (uint16_t)this->visible_comp_sensor_->get_state(); 
    uint16_t ir_comp = (uint16_t)this->ir_comp_sensor_->get_state(); 
    return (float)(rawuvb) - ( ( VEML6075_UVB1_COEFF * VEML6075_UV_BETA * visible_comp) / VEML6075_UV_GAMMA ) - ( (VEML6075_UVB2_COEFF * VEML6075_UV_BETA * ir_comp) / VEML6075_UV_DELTA );	
}	

float VEML6075Component::calc_uvindex(void){
    float index;
    float uva = (float) this->uva_sensor_->get_state();
    float uvb = (float) this->uvb_sensor_->get_state();	
    float uva_responsivity = (float) this->uva_responsivity_;
    float uvb_responsivity = (float) this->uvb_responsivity_;
    bool hdenabled = (bool) this->hdenabled_;
	
    float uvia = (uva) * (1.0 / VEML6075_UV_ALPHA) * ( uva_responsivity );
    float uvib = (uvb) * (1.0 / VEML6075_UV_BETA)  * ( uvb_responsivity );
    index      = (uvia + uvib) / 2.0;
    if (hdenabled == true)
    {
        index *= VEML6075_HD_SCALAR;
    }
    return index;
}	
	
void VEML6075Component::read_data_() {
  uint8_t data[2];	
  uint16_t visible_compensation , ir_compensation;
  uint16_t rawuva , rawuvb;
  float uva , uvb , uvindex;
	
  ESP_LOGD(TAG, "will read visible comp register");
 /*
  uint16_t conf;  
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
    ESP_LOGE(TAG, "Can't communicate with VEML6075 for the VEML6075_REG_CONF register in high dynamic");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG , "VEML6075_REG_CONF: data[0]= %d, data[1]= %d" , data[0] , data[1]); 
 */

/*	
  visible_compensation  = calc_visible_comp();
  if (this->visible_comp_sensor_ != nullptr) {
	  this->visible_comp_sensor_->publish_state(visible_compensation);
	  ESP_LOGD(TAG, "visible_compensation: %d" , visible_compensation);
  }  

 	
  ir_compensation       = calc_ir_comp();
  if (this->ir_comp_sensor_ != nullptr) {
	  this->ir_comp_sensor_->publish_state(ir_compensation);
	  ESP_LOGD(TAG, "ir_compensation: %d" , ir_compensation);
  }
	
  rawuva                = calc_rawuva();
  if (this->rawuva_sensor_ != nullptr) {
	  this->rawuva_sensor_->publish_state(rawuva);
	  ESP_LOGD(TAG, "raw UVA: %d" , rawuva);
  }

  rawuvb                = calc_rawuvb();
  if (this->rawuvb_sensor_ != nullptr) {
	  this->rawuvb_sensor_->publish_state(rawuvb);
	  ESP_LOGD(TAG, "raw UVB: %d" , rawuvb);
  }
	
  uva                  = calc_uva();
  if (this->uva_sensor_ != nullptr) {
	  this->uva_sensor_->publish_state(uva);
	  ESP_LOGD(TAG, "UVA: %f" , uva);
  }
	
  uvb                  = calc_uvb();
  if (this->uva_sensor_ != nullptr) {
	  this->uvb_sensor_->publish_state(uvb);
	  ESP_LOGD(TAG, "UVB: %f" , uvb);
  }
	
  uvindex              = calc_uvindex();
   if (this->uvindex_sensor_ != nullptr) {
	  this->uvindex_sensor_->publish_state(uvindex);
	  ESP_LOGD(TAG, "UV index: %f" , uvindex);
  }	
*/	
}
	


}  // namespace veml6075
}  // namespace esphome
