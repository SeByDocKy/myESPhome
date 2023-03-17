#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
//#include "esphome/core/optional.h"

namespace esphome {
namespace veml6075 {

// enums from https://github.com/sparkfun/SparkFun_VEML6075_Arduino_Library/tree/master/src

static const uint8_t VEML6075_ADDR                   = 0x10;
static const uint8_t VEML6075_REG_CONF               = 0x00;
static const uint8_t VEML6075_REG_UV_A               = 0x07;
static const uint8_t VEML6075_REG_DARK               = 0x08;
static const uint8_t VEML6075_REG_UV_B               = 0x09;
static const uint8_t VEML6075_REG_UV_UVCOMP1         = 0x0A;
static const uint8_t VEML6075_REG_UV_UVCOMP2         = 0x0B;
static const uint8_t VEML6075_REG_ID                 = 0x0C;
static const uint8_t VEML6075_ID                     = 0x26;
	
static const uint8_t VEML6075_SHUTDOWN_MASK           = 0x01;
static const uint8_t VEML6075_SHUTDOWN_SHIFT          = 0;	

static const uint8_t VEML6075_AF_MASK                 = 0x02;
static const uint8_t VEML6075_AF_SHIFT                = 1;	

static const uint8_t VEML6075_TRIG_MASK               = 0x04;
static const uint8_t VEML6075_TRIG_SHIFT              = 2;	
	
static const uint8_t VEML6075_HD_MASK                 = 0x08;
static const uint8_t VEML6075_HD_SHIFT                = 3;	

static const uint8_t VEML6075_UV_IT_MASK              = 0x70;
static const uint8_t VEML6075_UV_IT_SHIFT             = 4;		

static const float VEML6075_DEFAULT_UVA1_COEFF        = 2.22;
static const float VEML6075_DEFAULT_UVA2_COEFF        = 1.33;
static const float VEML6075_DEFAULT_UVB1_COEFF        = 2.95;
static const float VEML6075_DEFAULT_UVB2_COEFF        = 1.74;
static const float VEML6075_DEFAULT_UVA_RESP          = 0.001461;
static const float VEML6075_DEFAULT_UVB_RESP          = 0.002591;
	
const float VEML6075_UVA_RESPONSIVITY_100MS_UNCOVERED = 0.001111;
const float VEML6075_UVB_RESPONSIVITY_100MS_UNCOVERED = 0.00125;
	
const int VEML6075_NUM_INTEGRATION_TIMES              = 5;

const float VEML6075_UVA_RESPONSIVITY[VEML6075_NUM_INTEGRATION_TIMES] =
    {
        VEML6075_UVA_RESPONSIVITY_100MS_UNCOVERED / 0.5016286645, // 50ms
        VEML6075_UVA_RESPONSIVITY_100MS_UNCOVERED,                // 100ms
        VEML6075_UVA_RESPONSIVITY_100MS_UNCOVERED / 2.039087948,  // 200ms
        VEML6075_UVA_RESPONSIVITY_100MS_UNCOVERED / 3.781758958,  // 400ms
        VEML6075_UVA_RESPONSIVITY_100MS_UNCOVERED / 7.371335505   // 800ms
};
	
const float VEML6075_UVB_RESPONSIVITY[VEML6075_NUM_INTEGRATION_TIMES] =
    {
        VEML6075_UVB_RESPONSIVITY_100MS_UNCOVERED / 0.5016286645, // 50ms
        VEML6075_UVB_RESPONSIVITY_100MS_UNCOVERED,                // 100ms
        VEML6075_UVB_RESPONSIVITY_100MS_UNCOVERED / 2.039087948,  // 200ms
        VEML6075_UVB_RESPONSIVITY_100MS_UNCOVERED / 3.781758958,  // 400ms
        VEML6075_UVB_RESPONSIVITY_100MS_UNCOVERED / 7.371335505   // 800ms
};
	
typedef enum
{
    VEML6075_ADDRESS = 0x10,
    VEML6075_ADDRESS_INVALID = 0xFF
} veml6075_address_t;

// VEML6075 error code returns:
typedef enum
{
    VEML6075_ERROR_READ = -4,
    VEML6075_ERROR_WRITE = -3,
    VEML6075_ERROR_INVALID_ADDRESS = -2,
    VEML6075_ERROR_UNDEFINED = -1,
    VEML6075_ERROR_SUCCESS = 1
} veml6075_error_t;

const veml6075_error_t VEML6075_SUCCESS = VEML6075_ERROR_SUCCESS;

typedef enum{
  NO_TRIGGER,
  TRIGGER_ONE_OR_UV_TRIG,
  TRIGGER_INVALID
} veml6075_uv_trig_t;

typedef enum{
  AF_DISABLE,
  AF_ENABLE,
  AF_INVALID
} veml6075_af_t;

// Sensing modes
typedef enum{
  IT_50MS,
  IT_100MS,
  IT_200MS,
  IT_400MS,
  IT_800MS,
  IT_RESERVED_0,
  IT_RESERVED_1,
  IT_RESERVED_2,
  IT_INVALID
} veml6075_uv_it_t;

typedef enum{
  DYNAMIC_NORMAL,
  DYNAMIC_HIGH,
  HD_INVALID
} veml6075_hd_t;


class VEML6075Component : public PollingComponent, public i2c::I2CDevice {
 public:
	
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  
  void set_uva_sensor(sensor::Sensor *uva_sensor) { this->uva_sensor_ = uva_sensor; }
  void set_uvb_sensor(sensor::Sensor *uvb_sensor) { this->uvb_sensor_ = uvb_sensor; }
  void set_uvindex_sensor(sensor::Sensor *uvindex_sensor) { this->uvindex_sensor_ = uvindex_sensor; }
  void set_visible_comp_sensor(sensor::Sensor *visible_comp_sensor) { this->visible_comp_sensor_ = visible_comp_sensor; }
  void set_ir_comp_sensor(sensor::Sensor *ir_comp_sensor) { this->ir_comp_sensor_ = ir_comp_sensor; }
  void set_rawuva_sensor(sensor::Sensor *rawuva_sensor) { this->rawuva_sensor_ = rawuva_sensor; }
  void set_rawuvb_sensor(sensor::Sensor *rawuvb_sensor) { this->rawuvb_sensor_ = rawuvb_sensor; }
	
  void set_integration_time(VEML6075_INTEGRATION_TIME_OPTIONS it) { this->it_ = it; }
  void set_dynamic(VEM6075_DYNAMIC_OPTIONS hd) { this->hd_ = hd; }
  void set_autoforce(VEM6075_AUTOFORCE_OPTIONS af) { this->af_ = af; }
	
//   void setcoefficients(float UVA1, float UVA2, float UVB1, float UVB1, float UVA_RESP, float UVB_RESP);
	 
  void identifychip(void);
  void shutdown(boolean stop);
  void forcedmode(veml6075_af_t af);
  void integrationtime(veml6075_uv_it_t it);
  void highdynamic(veml6075_hd_t hd);
	
  uint16_t calc_visible_comp(void);
  uint16_t calc_ir_comp(void);
   
protected:
	
  sensor::Sensor *uva_sensor_{nullptr};
  sensor::Sensor *uvb_sensor_{nullptr};
  sensor::Sensor *uvindex_sensor_{nullptr};  
  sensor::Sensor *visible_comp_sensor_{nullptr};
  sensor::Sensor *ir_comp_sensor_{nullptr};
  sensor::Sensor *rawuva_sensor_{nullptr};
  sensor::Sensor *rawuvb_sensor_{nullptr};
  
  veml6075_uv_it_t it_{IT_100MS};
  veml6075_af_t af_{AF_DISABLE};
  veml6075_hd_t hd_{DYNAMIC_NORMAL};
	
  float uva1_ = VEML6075_DEFAULT_UVA1_COEFF, uva2_ = VEML6075_DEFAULT_UVA2_COEFF;
  float uvb1_ = VEML6075_DEFAULT_UVB1_COEFF, uvb2_ = VEML6075_DEFAULT_UVB1_COEFF;
  float uva_resp_ = VEML6075_DEFAULT_UVA_RESP, uvb_resp_ = VEML6075_DEFAULT_UVB_RESP;
  float uva_calc_, uvb_calc_;
	
  float uva_responsivity_, uvv_responsivity_;
  uint16_t  integrationtime_;
  bool hdenabled_;
	
/* 
  typedef enum{
   REG_UV_CONF = 0x00,
   REG_UVA_DATA = 0x07,
   REG_UVB_DATA = 0x09,
   REG_UVCOMP1_DATA = 0x0A,
   REG_UVCOMP2_DATA = 0x0B,
   REG_ID = 0x0C
  } veml6075_register_t;
  
  veml6075_address_t _deviceAddress;
*/  

/*	
  uint16_t rawbva(void);
  uint16_t rawbvb(void);
  float uva(void);
  float uvb(void);
  float index(void);
	
  float a(void);
  float b(void);
  float i(void);

  uint16_t uvcomp1(void);
  uint16_t uvcomp2(void);
  uint16_t visibleCompensation(void);
  uint16_t irCompensation(void);
  */
  
 /* 
  boolean isConnected(void);
  
  veml6075_error_t setIntegrationTime(veml6075_uv_it_t it);
  veml6075_uv_it_t getIntegrationTime(void);
  veml6075_error_t setHighDynamic(veml6075_hd_t hd);
  veml6075_hd_t getHighDynamic(void);
  veml6075_error_t setTrigger(veml6075_uv_trig_t trig);
  veml6075_uv_trig_t getTrigger(void);
  veml6075_error_t trigger(void);

  veml6075_error_t setAutoForce(veml6075_af_t af);
  veml6075_af_t getAutoForce(void);

  veml6075_error_t powerOn(boolean enable = true);
  veml6075_error_t shutdown(boolean shutdown = true);


  veml6075_error_t deviceID(uint8_t *id);
  veml6075_error_t deviceAddress(uint8_t *address);
  */
  
  
/*	
 private:
 
  
  void takeReading(void);

  uint16_t _read_delay;

  float _uv_a_1, _uv_a_2, _uv_b_1, _uv_b_2, _uv_a_resp, _uv_b_resp;
  float _uv_a_calc, _uv_b_calc; 
  
  veml6075_commandRegister _commandRegister;
*/

 
};

}  // namespace veml6075
}  // namespace esphome
