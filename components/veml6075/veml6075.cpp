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
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with VEML6075 failed!");
  }
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
  
/*	
  identifychip(); // check if it's a genuine chip
 
  shutdown(true); // Shut down to change settings   VEML6075_REG_CONF(0x00) bit0-MSB/bit8 16 bit
*/  
  write_reg_00();	

  // Set Force readings
  forcedmode(this->af_); // autoforce/enable trigger  VEML6075_REG_CONF(0x00) bit1-MSB/bit9 16 bit
	
  // Set trigger mode
  trigger(this->trig_); // trigger mode  VEML6075_REG_CONF(0x00) bit2-MSB/bit10 16 bit
	
  // Set high dynamic
  highdynamic(this->hd_); // high dynamic  VEML6075_REG_CONF(0x00) bit3-MSB/bit11 16 bit	
  
  // Set integration time
  integrationtime(this->it_); // integration time  VEML6075_REG_CONF(0x00) bits 6-5-4-MSB/bits 14-13-12 16 bit
	
  shutdown(false); // Turn on chip after settings set

/*	
//	*/ 
}

void VEML6075Component::update() { 
	ESP_LOGVV(TAG, "in update() for VEML6075...");
	this->read_data_(); 
}	

void VEML6075Component::write_reg_00(void){
  uint8_t data[2] = {0,0}; 
  this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE);	
  ESP_LOGVV(TAG, "write_bytes O into VEML6075_REG_CONF successfully");	
}	
	
uint8_t VEML6075Component::read_reg_00(void){
  uint8_t data[2] = {0,0}; 
  this->read_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  ESP_LOGVV(TAG, "read before masking shutdown %d %d" , data[0] , data[1]);	
  return data[0];	
}
	
void VEML6075Component::identifychip(void){
  uint8_t data[2] = {0,0};
  uint16_t conf;
  this->read_register(VEML6075_REG_ID, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
	
  if (data[0] != VEML6075_ID) {
    ESP_LOGE(TAG, "Wrong ID, received %d, expecting %d", data[0] , VEML6075_ID);
    return;
  }
  ESP_LOGD(TAG, "Chip identification successfull, received %d, expecting %d", data[0] , VEML6075_ID);
  
}
	 
void VEML6075Component::shutdown(bool stop){
  uint8_t data[2] = {0,0};
  uint16_t conf , sd = 0;
  
  this->read_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  ESP_LOGVV(TAG, "read before masking shutdown %d %d" , data[1] , data[0]);	
	
  conf  = (data[0] | (data[1] << 8));	
  ESP_LOGVV(TAG, "read VEML6075_REG_CONF: %d" , conf);
  
  if (stop == true){ sd = (uint16_t)1;}
  ESP_LOGVV(TAG, "sd value: %d" , sd);
  ESP_LOGVV(TAG, "VEML6075_SHUTDOWN_MASK: %d" ,VEML6075_SHUTDOWN_MASK);
  
  conf &= ~(VEML6075_SHUTDOWN_MASK);     // Clear shutdown bit
  ESP_LOGVV(TAG, "conf = conf & ~(VEML6075_SHUTDOWN_MASK): %d" , conf);	
 
  conf |= (sd << VEML6075_SHUTDOWN_SHIFT); //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGVV(TAG, " set conf |= sd << VEML6075_SHUTDOWN_SHIFT to: %d" , conf);
  
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGVV(TAG, "write after masking shutdown %d %d" , data[1] , data[0]);	

  //this->write_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  //this->write_register(VEML6075_REG_CONF, data , (size_t)VEML6075_REG_SIZE , true);		
  this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE);
  ESP_LOGVV(TAG, "write_byte with VEML6075_REG_CONF successfull to turn on/off chip");
  
}
 
void VEML6075Component::forcedmode(veml6075_af_t af){
  uint8_t data[2]= {0,0};
  uint16_t conf;
	
  this->read_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
	
  ESP_LOGVV(TAG, "af: %d" , af);
  ESP_LOGVV(TAG, "read before masking force mode %d %d" , data[1] , data[0]);
  conf  = (data[0] | (data[1] << 8));
  ESP_LOGVV(TAG, "read VEML6075_REG_CONF: %d" , conf);
  conf &= ~(VEML6075_AF_MASK);     // Clear shutdown bit
  ESP_LOGVV(TAG, "conf = conf & ~(VEML6075_AF_MASK): %d" , conf);
  conf |= (af << VEML6075_AF_SHIFT); //VEML6075_MASK(conf, VEML6075_AF_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGVV(TAG, "set conf |= af << VEML6075_AF_SHIFT to: %d" , conf);
	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8); 	
  ESP_LOGVV(TAG, "Wil write VEML6075_REG_CONF after masking force mode with: %d %d" , data[1] , data[0]);
  
  //this->write_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  //this->write_register(VEML6075_REG_CONF, data , (size_t)VEML6075_REG_SIZE , true);	
  this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE);	
  ESP_LOGVV(TAG, "write_bytes with VEML6075_REG_CONF successfull to set autoforce mode");
}

void VEML6075Component::trigger(veml6075_uv_trig_t trig) {
  uint8_t data[2]= {0,0};
  uint16_t conf;
  this->read_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  ESP_LOGVV(TAG, "trig: %d" , trig);
  ESP_LOGVV(TAG, "read before masking trigger %d %d" , data[1] , data[0]);
  conf  = (data[0] | (data[1] << 8));
  ESP_LOGVV(TAG, "read VEML6075_REG_CONF: %d" , conf);
  conf &= ~(VEML6075_TRIG_MASK);     // Clear shutdown bit
  ESP_LOGVV(TAG, "conf = conf & ~(VEML6075_TRIG_MASK): %d" , conf);	
  conf |= (trig << VEML6075_TRIG_SHIFT); //VEML6075_MASK(conf, VEML6075_TRIG_MASK, VEML6075_TRIG_SHIFT);
  ESP_LOGVV(TAG, "set conf |= trig << VEML6075_TRIG_SHIFT to: %d" , conf);	
	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGVV(TAG, "Wil write VEML6075_REG_CONF after masking trigger  with: %d %d" , data[1] , data[0]);
	
  //this->write_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  //this->write_register(VEML6075_REG_CONF, data , (size_t)VEML6075_REG_SIZE , true);
  this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE);	
  ESP_LOGVV(TAG, "write_bytes with VEML6075_REG_CONF successfull to set trigger mode");
}

void VEML6075Component::highdynamic(veml6075_hd_t hd){
  uint8_t data[2]= {0,0};
  uint16_t conf;
  
  this->read_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  ESP_LOGVV(TAG, "read before masking high dynamic %d %d" , data[1] , data[0]);
  conf  = (data[0] | (data[1] << 8));
  ESP_LOGVV(TAG, "read VEML6075_REG_CONF: %d" , conf);
  if (hd == DYNAMIC_HIGH){
        this->hdenabled_ = true;
  }
  else{
        this->hdenabled_ = false;
  }
  
  conf &= ~(VEML6075_HD_MASK);     // Clear shutdown bit
  ESP_LOGVV(TAG, "conf = conf & ~(VEML6075_HD_MASK): %d" , conf);
  conf |= hd << VEML6075_HD_SHIFT; 
  ESP_LOGVV(TAG, "set conf |= hd << VEML6075_HD_SHIFT to: %d" , conf);	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGVV(TAG, "Wil write VEML6075_REG_CONF after masking high dynamic with: %d %d" , data[1] , data[0]);
  //this->write_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , fase);
  //this->write_register(VEML6075_REG_CONF, data , (size_t)VEML6075_REG_SIZE , true);	
  this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE);	
  ESP_LOGVV(TAG, "write_byte with VEML6075_REG_CONF successfull to set high dynamic mode");
}  
		
void VEML6075Component::integrationtime(veml6075_uv_it_t it){
  uint8_t data[2]= {0,0};
  uint16_t conf;
  this->read_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  
  ESP_LOGVV(TAG, "it: %d" , it);
  ESP_LOGVV(TAG, "read before masking integration time %d %d" , data[1] , data[0]);
  conf  = (data[0] | (data[1] << 8));
  ESP_LOGVV(TAG, "read VEML6075_REG_CONF: %d" , conf);
  conf &= ~(VEML6075_UV_IT_MASK);     // Clear shutdown bit
  ESP_LOGVV(TAG, "conf = conf & ~(VEML6075_UV_UT_MASK): %d" , conf);
  conf |= (it << VEML6075_UV_IT_SHIFT); //VEML6075_MASK(conf, VEML6075_SHUTDOWN_MASK, VEML6075_SHUTDOWN_SHIFT);
  ESP_LOGVV(TAG, "set conf |= it << VEML6075_UV_IT_SHIFT to: %d" , conf);	
  data[0] = (uint8_t)(conf & 0x00FF);
  data[1] = (uint8_t)((conf & 0xFF00) >> 8);
  ESP_LOGVV(TAG, "Wil write VEML6075_REG_CONF after masking integration time  with: %d %d" , data[1] , data[0]);
  //this->write_register(VEML6075_REG_CONF, (uint8_t *) &data , (size_t)VEML6075_REG_SIZE , false);
  //this->write_register(VEML6075_REG_CONF, data , (size_t)VEML6075_REG_SIZE , true);	
  this->write_bytes(VEML6075_REG_CONF, data , VEML6075_REG_SIZE);	
  ESP_LOGVV(TAG, "write_bytes with VEML6075_REG_CONF successfull to set integration time mode");
	
  this->uva_responsivity_ = (float)VEML6075_UVA_RESPONSIVITY[(uint8_t)it];
  this->uvb_responsivity_ = (float)VEML6075_UVB_RESPONSIVITY[(uint8_t)it];
  ESP_LOGVV(TAG, "Responsability UVA et UVB %f , %f" , this->uva_responsivity_ , this->uvb_responsivity_);
		
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

uint16_t VEML6075Component::calc_rawuva(void){
    uint8_t data[2];
    uint8_t dummy[1];	
    uint16_t result;
    this->read_register(VEML6075_REG_UVA , (uint8_t *) &data, (size_t)VEML6075_REG_SIZE , false);
    result = (data[0] | (data[1] << 8)); 
    ESP_LOGVV(TAG , "calc_rawuva read successfully data[0]: %d, data[1]: %d, result: %d" , data[0] , data[1] , result);
    this->read_register(VEML6075_REG_UVA , (uint8_t *) &dummy, (size_t)1 , true);	 // dummy reading to close connexion	
    return result;	    
}
	
uint16_t VEML6075Component::calc_rawuvb(void){
    uint8_t data[2] = {0, 0};
    uint8_t dummy[1];	
    uint16_t result;	
    this->read_register(VEML6075_REG_UVB , (uint8_t *) &data, (size_t) VEML6075_REG_SIZE , false);
    result = (data[0] | (data[1] << 8)); 
    ESP_LOGVV(TAG , "calc_rawuvb read successfully data[0]: %d, data[1]: %d, result: %d" , data[0] , data[1] , result); 
    this->read_register(VEML6075_REG_UVA , (uint8_t *) &dummy, (size_t)1 , true);	 // dummy reading to close connexion
    return result;	    
}
	
uint16_t VEML6075Component::calc_visible_comp(void){
    uint8_t data[2] = {0,0};
    uint8_t dummy[1];	
    uint16_t result;
    this->read_register(VEML6075_REG_VISIBLE_COMP, (uint8_t *) &data, (size_t) VEML6075_REG_SIZE , false);
    result = (data[0] | (data[1] << 8)); 
    ESP_LOGVV(TAG , "calc_visible_comp read successfully data[0]: %d, data[1]: %d, result: %d" , data[0] , data[1] , result);
    this->read_register(VEML6075_REG_UVA , (uint8_t *) &dummy, (size_t)1 , true);	 // dummy reading to close connexion
    return result;
}

uint16_t VEML6075Component::calc_ir_comp(void){
    uint8_t data[2] = {0, 0};
    uint8_t dummy[1];	
    uint16_t result;
    this->read_register(VEML6075_REG_IR_COMP, (uint8_t *) &data, (size_t)VEML6075_REG_SIZE , false);
    result = (data[0] | (data[1] << 8)); 
    ESP_LOGVV(TAG , "calc_ir_comp read successfully data[0]: %d, data[1]: %d, result: %d" , data[0] , data[1] , result);
    this->read_register(VEML6075_REG_UVA , (uint8_t *) &dummy, (size_t)1 , true);	 // dummy reading to close connexion
    return result;
}
			
float VEML6075Component::calc_uva(void){
    float rawuva       = (float)this->rawuva_sensor_->get_state();
    float visible_comp = (float)this->visible_comp_sensor_->get_state(); 
    float ir_comp      = (float)this->ir_comp_sensor_->get_state(); 
    return ( rawuva -  ( ( VEML6075_UVA1_COEFF * VEML6075_UV_ALPHA * visible_comp) / VEML6075_UV_GAMMA ) - ( (VEML6075_UVA2_COEFF * VEML6075_UV_ALPHA * ir_comp) / VEML6075_UV_DELTA ) );
}

float VEML6075Component::calc_uvb(void){
    float rawuvb       =  (float)this->rawuvb_sensor_->get_state();
    float visible_comp =  (float)this->visible_comp_sensor_->get_state(); 
    float ir_comp      =  (float)this->ir_comp_sensor_->get_state(); 
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
  uint16_t visible_compensation , ir_compensation;
  uint16_t rawuva , rawuvb;
  float uva , uvb , uvindex;
 
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
