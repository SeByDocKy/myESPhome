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

	
//  uint8_t data[2] = {0,0};
//  uint16_t rawuva;
  ESP_LOGCONFIG(TAG, "Setting up VEML6075...");
  
/*	
  data[0] = 0;
  data[1] = 0;
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE )) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to turn on/off chip");
  }
  else{
     ESP_LOGD(TAG, "write_bytes 0 into VEML6075_REG_CONF successfully ");
  }
*/
/*	
  data[0] = 15;
  data[1] = 0;
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE )) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to turn on/off chip");
  }
  else{
     ESP_LOGD(TAG, "write_bytes with VEML6075_REG_CONF successfully ");
  }
  data[0] = 14;
  data[1] = 0;
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE )) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to turn on/off chip");
  }
  else{
     ESP_LOGD(TAG, "write_bytes with VEML6075_REG_CONF successfully ");
  }

*/
//  */
	
  /*   
  identifychip(); // check if it's a genuine chip
 
*/ 
 /* 
  shutdown(true); // Shut down to change settings   VEML6075_REG_CONF(0x00) bit0-MSB/bit8 16 bit
 
  // Set Force readings
  forcedmode(this->af_); // autoforce/enable trigger  VEML6075_REG_CONF(0x00) bit1-MSB/bit9 16 bit
	
  // Set trigger mode
  trigger(this->trig_); // trigger mode  VEML6075_REG_CONF(0x00) bit2-MSB/bit10 16 bit
	
  // Set high dynamic
  highdynamic(this->hd_); // high dynamic  VEML6075_REG_CONF(0x00) bit3-MSB/bit11 16 bit	
  
  // Set integration time
  integrationtime(this->it_); // integration time  VEML6075_REG_CONF(0x00) bits 6-5-4-MSB/bits 14-13-12 16 bit

  shutdown(false); // Turn on chip after settings set
 */
// /*	
 // shutdown(true); // Shut down to change settings   VEML6075_REG_CONF(0x00) bit0-MSB/bit8 16 bit
	
 write_reg_00(false , this->af_ , this->trig_ , this->hd_ , this->it_);
//  */
}

void VEML6075Component::update() { 
	ESP_LOGD(TAG, "in update() for VEML6075...");
	this->read_data_(); 
}	

void VEML6075Component::identifychip(void){
  uint8_t data[2] = {0,0};
  uint16_t conf;
  
  if ( !this->read_bytes(VEML6075_REG_ID, (uint8_t *) &data , VEML6075_REG_SIZE) ) {   //
    ESP_LOGE(TAG, "Can't read  VEML6075_REG_ID");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "read REG_ID register %d %d" , data[1] , data[0]);
  
  if (data[0] != VEML6075_ID) {
    ESP_LOGE(TAG, "Wrong ID, received %d, expecting %d", data[0] , VEML6075_ID);
    return;
  }
  ESP_LOGD(TAG, "Chip identification successfull, received %d, expecting %d", data[0] , VEML6075_ID);
  
}
	
void VEML6075Component::write_reg_00(bool stop , veml6075_af_t af , veml6075_uv_trig_t trig , veml6075_hd_t hd , veml6075_uv_it_t it){
  uint8_t data[2] = {0,0};
  uint16_t conf = 0 , sd = 0;
  if (stop == true){ sd = (uint16_t)1;}
  if (hd == DYNAMIC_HIGH){
        this->hdenabled_ = true;
  }
  else{
        this->hdenabled_ = false;
  }
  this->uva_responsivity_ = (float)VEML6075_UVA_RESPONSIVITY[(uint8_t)it];
  this->uvb_responsivity_ = (float)VEML6075_UVB_RESPONSIVITY[(uint8_t)it];
  
  ESP_LOGD(TAG, "sd value: %d" , 1);
  conf &= ~(VEML6075_SHUTDOWN_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_SHUTDOWN_MASK): %d" , conf);	 
  conf |= (1 << VEML6075_SHUTDOWN_SHIFT); //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, "set conf |= sd << VEML6075_SHUTDOWN_SHIFT to: %d" , conf);

/*	
  ESP_LOGD(TAG, "sd value: %d" , sd);
  conf &= ~(VEML6075_SHUTDOWN_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_SHUTDOWN_MASK): %d" , conf);	 
  conf |= (sd << VEML6075_SHUTDOWN_SHIFT); //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, "set conf |= sd << VEML6075_SHUTDOWN_SHIFT to: %d" , conf);
*/	
  ESP_LOGD(TAG, "af value: %d" , af);
  conf &= ~(VEML6075_AF_MASK);     // Clear af bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_AF_MASK): %d" , conf);
  conf |= (af << VEML6075_AF_SHIFT); //VEML6075_MASK(conf, VEML6075_AF_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, "set conf |= af << VEML6075_AF_SHIFT to: %d" , conf);
  
  ESP_LOGD(TAG, "trig value: %d" , trig);
  conf &= ~(VEML6075_TRIG_MASK);     // Clear trigger bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_TRIG_MASK): %d" , conf);	
  conf |= (trig << VEML6075_TRIG_SHIFT); //VEML6075_MASK(conf, VEML6075_TRIG_MASK, VEML6075_TRIG_SHIFT);
  ESP_LOGD(TAG, "set conf |= trig << VEML6075_TRIG_SHIFT to: %d" , conf);	

  ESP_LOGD(TAG, "hd value: %d" , hd);
  conf &= ~(VEML6075_HD_MASK);     // Clear hd bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_HD_MASK): %d" , conf);
  conf |= (hd << VEML6075_HD_SHIFT); 
  ESP_LOGD(TAG, "set conf |= hd << VEML6075_HD_SHIFT to: %d" , conf);	
	
  ESP_LOGD(TAG, "it value: %d" , it);
  conf &= ~(VEML6075_UV_IT_MASK);     // Clear integration time bits
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_UV_UT_MASK): %d" , conf);
  conf |= (it << VEML6075_UV_IT_SHIFT); //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, "set conf |= it << VEML6075_UV_IT_SHIFT to: %d" , conf);
	
  data[1] = (uint8_t)(conf & 0x00FF);
  data[0] = (uint8_t)((conf & 0xFF00) >> 8); 	
  ESP_LOGD(TAG, "Wil write VEML6075_REG_CONF with: %d %d" , data[0] , data[1]);
	
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed");
     return;
  }
  ESP_LOGD(TAG, "write_bytes with VEML6075_REG_CONF successfully");
	
  ESP_LOGD(TAG, "sd value: %d" , 0);
  conf &= ~(VEML6075_SHUTDOWN_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_SHUTDOWN_MASK): %d" , conf);	 
  conf |= (0 << VEML6075_SHUTDOWN_SHIFT); //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, "set conf |= sd << VEML6075_SHUTDOWN_SHIFT to: %d" , conf);
	
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed");
     return;
  }
  ESP_LOGD(TAG, "write_bytes with VEML6075_REG_CONF successfully");
	
}
 
void VEML6075Component::shutdown(bool stop){
  uint8_t data[2] = {0,0};
  uint16_t conf , sd = 0;
  
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
    ESP_LOGE(TAG, "Can't read initial VEML6075_REG_CONF in shutdown ");
//    this->mark_failed();
//    return;
  }
  ESP_LOGD(TAG, "read before masking shutdown %d %d" , data[1] , data[0]);	
	
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));	
  ESP_LOGD(TAG, "read VEML6075_REG_CONF: %d" , conf);
  
  if (stop == true){ sd = (uint16_t)1;}
  ESP_LOGD(TAG, "sd value: %d" , sd);
  ESP_LOGD(TAG, "VEML6075_SHUTDOWN_MASK: %d" ,VEML6075_SHUTDOWN_MASK);
  
  conf &= ~(VEML6075_SHUTDOWN_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_SHUTDOWN_MASK): %d" , conf);	
 
  conf |= (sd << VEML6075_SHUTDOWN_SHIFT); //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, " set conf |= sd << VEML6075_SHUTDOWN_SHIFT to: %d" , conf);
  
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGD(TAG, "write after masking shutdown %d %d" , data[1] , data[0]);	

  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE )) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to turn on/off chip");
     return;
  }
  ESP_LOGD(TAG, "write_byte with VEML6075_REG_CONF successfull to turn on/off chip");
  
}
 
void VEML6075Component::forcedmode(veml6075_af_t af){
  uint8_t data[2]= {0,0};
  uint16_t conf;
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
    ESP_LOGE(TAG, "Can't read initial VEML6075_REG_CONF in forcemode");
  //  this->mark_failed();
  //  return;
  }
  ESP_LOGD(TAG, "af: %d" , af);
  ESP_LOGD(TAG, "read before masking force mode %d %d" , data[1] , data[0]);
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));
  ESP_LOGD(TAG, "read VEML6075_REG_CONF: %d" , conf);
  conf &= ~(VEML6075_AF_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_AF_MASK): %d" , conf);
  conf |= (af << VEML6075_AF_SHIFT); //VEML6075_MASK(conf, VEML6075_AF_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, "set conf |= af << VEML6075_AF_SHIFT to: %d" , conf);
	
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
    ESP_LOGE(TAG, "Can't read initial VEML6075_REG_CONF in trigger mode");
//    this->mark_failed();
  //  return;
  }
  ESP_LOGD(TAG, "trig: %d" , trig);
  ESP_LOGD(TAG, "read before masking trigger %d %d" , data[1] , data[0]);
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));
  ESP_LOGD(TAG, "read VEML6075_REG_CONF: %d" , conf);
  conf &= ~(VEML6075_TRIG_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_TRIG_MASK): %d" , conf);	
  conf |= (trig << VEML6075_TRIG_SHIFT); //VEML6075_MASK(conf, VEML6075_TRIG_MASK, VEML6075_TRIG_SHIFT);
  ESP_LOGD(TAG, "set conf |= trig << VEML6075_TRIG_SHIFT to: %d" , conf);	
	
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

void VEML6075Component::highdynamic(veml6075_hd_t hd){
  uint8_t data[2]= {0,0};
  uint16_t conf;
  
  ESP_LOGD(TAG, "hd: %d" , hd);
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
    ESP_LOGE(TAG, "Can't read initial VEML6075_REG_CONF in high dynamic");
//    this->mark_failed();
//    return;
  }
  ESP_LOGD(TAG, "read before masking high dynamic %d %d" , data[1] , data[0]);
	
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));
  ESP_LOGD(TAG, "read VEML6075_REG_CONF: %d" , conf);
  if (hd == DYNAMIC_HIGH){
        this->hdenabled_ = true;
  }
  else{
        this->hdenabled_ = false;
  }
  
  conf &= ~(VEML6075_HD_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_HD_MASK): %d" , conf);
  conf |= hd << VEML6075_HD_SHIFT; 
  ESP_LOGD(TAG, "set conf |= hd << VEML6075_HD_SHIFT to: %d" , conf);	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGD(TAG, "Wil write VEML6075_REG_CONF after masking high dynamic with: %d %d" , data[1] , data[0]);
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set high dynamic mode");
     return;
  }
  ESP_LOGD(TAG, "write_byte with VEML6075_REG_CONF successfull to set high dynamic mode");
}  
	
	
void VEML6075Component::integrationtime(veml6075_uv_it_t it){
  uint8_t data[2]= {0,0};
  uint16_t conf;
  if (!this->read_bytes(VEML6075_REG_CONF, (uint8_t *) &data , VEML6075_REG_SIZE)) {
    ESP_LOGE(TAG, "Can't read initial VEML6075_REG_CONF in integration time");
//    this->mark_failed();
 //   return;
  }
  ESP_LOGD(TAG, "it: %d" , it);
  ESP_LOGD(TAG, "read before masking integration time %d %d" , data[1] , data[0]);
  conf  = ((data[0]  & 0x00FF) | ((data[1]  & 0x00FF) << 8));
  ESP_LOGD(TAG, "read VEML6075_REG_CONF: %d" , conf);
  conf &= ~(VEML6075_UV_IT_MASK);     // Clear shutdown bit
  ESP_LOGD(TAG, "conf = conf & ~(VEML6075_UV_UT_MASK): %d" , conf);
  conf |= (it << VEML6075_UV_IT_SHIFT); //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGD(TAG, "set conf |= it << VEML6075_UV_IT_SHIFT to: %d" , conf);	
	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  
  ESP_LOGD(TAG, "Wil write VEML6075_REG_CONF after masking integration time  with: %d %d" , data[1] , data[0]);
  if (!this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE)) {
     ESP_LOGW(TAG, "write_byte with VEML6075_REG_CONF failed to set integration time mode");
     return;
  }
  ESP_LOGD(TAG, "write_bytes with VEML6075_REG_CONF successfull to set integration time mode");
  
  this->uva_responsivity_ = (float)VEML6075_UVA_RESPONSIVITY[(uint8_t)it];
  this->uvb_responsivity_ = (float)VEML6075_UVB_RESPONSIVITY[(uint8_t)it];
  ESP_LOGD(TAG, "Responsability UVA et UVB %f , %f" , this->uva_responsivity_ , this->uvb_responsivity_);
		
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
        this->integrationtime_ = 100;
    } 
}


float VEML6075Component::calc_visible_comp(void){
    uint8_t data[2] = {0,0};
    float result;
    if (!this->read_bytes(VEML6075_REG_VISIBLE_COMP, (uint8_t *) &data, VEML6075_REG_SIZE)){
       ESP_LOGE(TAG, "can't read VEML6075_REG_VISIBLE_COMP register");
        return NAN;	  
    }
    result = (float)((data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8)); 
    ESP_LOGD(TAG , "calc_visible_comp read successfully data[0]: %d, data[1]: %d, result: %f" , data[0] , data[1] , result); 
    return result;
}

float VEML6075Component::calc_ir_comp(void){
    uint8_t data[2] = {0, 0};
    float result;
    if (!this->read_bytes(VEML6075_REG_IR_COMP, (uint8_t *) &data, VEML6075_REG_SIZE)){
       ESP_LOGE(TAG, "can't read VEML6075_REG_IR_COMP register");
       return NAN;	  
    }
    result = (float)((data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8)); 
    ESP_LOGD(TAG , "calc_ir_comp read successfully data[0]: %d, data[1]: %d, result: %f" , data[0] , data[1] , result); 
    return result;
}
		
float VEML6075Component::calc_rawuva(void){
    uint8_t data[2] = {0, 0};
    float result;
    if (!this->read_bytes(VEML6075_REG_UVA , (uint8_t *) &data, VEML6075_REG_SIZE)){
       ESP_LOGE(TAG, "can't read VEML6075_REG_UVA  register");
       return NAN;
    }
    result = (float)((data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8)); 
    ESP_LOGD(TAG , "calc_rawuva read successfully data[0]: %d, data[1]: %d, result: %f" , data[0] , data[1] , result); 
    return result;	    
}
	
float VEML6075Component::calc_rawuvb(void){
    uint8_t data[2] = {0, 0};
    float result;
    if (!this->read_bytes(VEML6075_REG_UVB , (uint8_t *) &data, VEML6075_REG_SIZE)){
       ESP_LOGE(TAG, "can't read VEML6075_REG_UVB  register");
       return NAN; 
    }
    result = (float)((data[0] & 0x00FF) | ((data[1] & 0x00FF) << 8));
    ESP_LOGD(TAG , "calc_rawuvb read successfully data[0]: %d, data[1]: %d, result: %f" , data[0] , data[1] , result); 	
    return result;	    
}
	
float VEML6075Component::calc_uva(void){
    float rawuva       = this->rawuva_sensor_->get_state();
    float visible_comp = this->visible_comp_sensor_->get_state(); 
    float ir_comp      = this->ir_comp_sensor_->get_state(); 
    return ( rawuva -  ( ( VEML6075_UVA1_COEFF * VEML6075_UV_ALPHA * visible_comp) / VEML6075_UV_GAMMA ) - ( (VEML6075_UVA2_COEFF * VEML6075_UV_ALPHA * ir_comp) / VEML6075_UV_DELTA ) );
    //         x    -            (2.22           *   1               *  x)/1  -  (1.33              *  1  * x)/1 ~ x - 3.55x = -2.55x
    //return (float)rawUva() - ( (UVA_A_COEF * UV_ALPHA * uvComp1()) / UV_GAMMA) - ((UVA_B_COEF * UV_ALPHA * uvComp2()) / UV_DELTA);
	
}

float VEML6075Component::calc_uvb(void){
    float rawuvb       =  this->rawuvb_sensor_->get_state();
    float visible_comp =  this->visible_comp_sensor_->get_state(); 
    float ir_comp      =  this->ir_comp_sensor_->get_state(); 
    return (rawuvb - ( ( VEML6075_UVB1_COEFF * VEML6075_UV_BETA * visible_comp) / VEML6075_UV_GAMMA ) - ( (VEML6075_UVB2_COEFF * VEML6075_UV_BETA * ir_comp) / VEML6075_UV_DELTA ));	
}	

float VEML6075Component::calc_uvindex(void){
	
    float uvia             = (this->uva_sensor_->get_state()) * (1.0 / VEML6075_UV_ALPHA) * ( this->uva_responsivity_ );
    float uvib             = (this->uvb_sensor_->get_state()) * (1.0 / VEML6075_UV_BETA)  * ( this->uvb_responsivity_ );
    float index            = (uvia + uvib) / 2.0;
    if (this->hdenabled_ == true)
    {
        index *= VEML6075_HD_SCALAR;
    }
    return index;
}	
	
void VEML6075Component::read_data_() {
  float visible_compensation , ir_compensation;
  float rawuva , rawuvb;
  float uva , uvb , uvindex;

  rawuva                = calc_rawuva();
  if (this->rawuva_sensor_ != nullptr) {
	  this->rawuva_sensor_->publish_state(rawuva);
	  ESP_LOGD(TAG, "raw UVA: %f" , rawuva);
  }
 
  rawuvb                = calc_rawuvb();
  if (this->rawuvb_sensor_ != nullptr) {
	  this->rawuvb_sensor_->publish_state(rawuvb);
	  ESP_LOGD(TAG, "raw UVB: %f" , rawuvb);
  }	
	
  visible_compensation  = calc_visible_comp();
  if (this->visible_comp_sensor_ != nullptr) {
	  this->visible_comp_sensor_->publish_state(visible_compensation);
	  ESP_LOGD(TAG, "visible_compensation: %f" , visible_compensation);
  }
 

  ir_compensation       = calc_ir_comp();
  if (this->ir_comp_sensor_ != nullptr) {
	  this->ir_comp_sensor_->publish_state(ir_compensation);
	  ESP_LOGD(TAG, "ir_compensation: %f" , ir_compensation);
  }
 
  
  uva                  = calc_uva();
  if (this->uva_sensor_ != nullptr) {
	  this->uva_sensor_->publish_state(uva);
	  ESP_LOGD(TAG, "UVA: %f" , uva);
  }
  
  uvb                  = calc_uvb();
  if (this->uvb_sensor_ != nullptr) {
	  this->uvb_sensor_->publish_state(uvb);
	  ESP_LOGD(TAG, "UVB: %f" , uvb);
  }	
	
  uvindex              = calc_uvindex();
   if (this->uvindex_sensor_ != nullptr) {
	  this->uvindex_sensor_->publish_state(uvindex);
	  ESP_LOGD(TAG, "UV index: %f" , uvindex);
  }	

}
	


}  // namespace veml6075
}  // namespace esphome
