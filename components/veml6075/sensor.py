import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["i2c"]

veml6075_ns = cg.esphome_ns.namespace("veml6075")
VEML6075Component = veml6075_ns.class_(
    "VEML6075Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_DEFAULT_I2C_ADRESS_SCHEMA         = 0x10
CONF_DEFAULT_POLLING_CONPONENT_SCHEMA  = "60s"

CONF_VEML6075_UVA                      = "uva"
CONF_VEML6075_UVB                      = "uvb"
CONF_VEML6075_UVINDEX                  = "uvindex"
CONF_VEML6075_VISIBLE_COMP             = "visible_comp"
CONF_VEML6075_IR_COMP                  = "ir_comp"
CONF_VEML6075_RAWUVA                   = "rawuva"
CONF_VEML6075_RAWUVB                   = "rawuva"

CONF_VEML6075_ICON_UV                  = "mdi:sun-wireless"
CONF_VEML6075_ICON_NUMERIC             = "mdi:numeric"

CONF_UNIT_UVA                          = "#/uW/cm²"
CONF_UNIT_UVB                          = "#/uW/cm²"
CONF_UNIT_UVINDEX                      = "mW/m2"


VEML6075_INTEGRATION_TIME = veml6075_ns.enum("veml6075_uv_it_t")
VEML6075_INTEGRATION_TIME_OPTIONS = {
    "50ms": VEML6075_INTEGRATION_TIME.IT_50MS,
    "100ms": VEML6075_INTEGRATION_TIME.IT_100MS,
    "200ms": VEML6075_INTEGRATION_TIME.IT_200MS,
    "400ms": VEML6075_INTEGRATION_TIME.IT_400MS,
    "800ms": VEML6075_INTEGRATION_TIME.IT_800MS,
}

VEML6075_DYNAMIC = veml6075_ns.enum("veml6075_hd_t")
VEML6075_DYNAMIC_OPTIONS = {
   "normal": VEML6075_DYNAMIC.DYNAMIC_NORMAL,
   "high": VEML6075_DYNAMIC.DYNAMIC_HIGH,
}

VEML6075_AUTOFORCE = veml6075_ns.enum("veml6075_af_t")
VEML6075_AUTOFORCE_OPTIONS = {
   "disable": VEML6075_AUTOFORCE.AF_DISABLE,
   "enable": VEML6075_AUTOFORCE.AF_ENABLE,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VEML6075Component),
		
            cv.Optional(CONF_VEML6075_UVA): sensor.sensor_schema(
                unit_of_measurement=CONF_UNIT_UVA,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
		icon=CONF_VEML6075_ICON_UV,
            ),
		
            cv.Optional(CONF_VEML6075_UVB): sensor.sensor_schema(
                unit_of_measurement=CONF_UNIT_UVB,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
		icon=CONF_VEML6075_ICON_UV,
            ),
		
	    cv.Optional(CONF_VEML6075_UVINDEX): sensor.sensor_schema(
                unit_of_measurement=CONF_UNIT_UVINDEX,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
		icon=CONF_VEML6075_ICON_NUMERIC,
            ),
		
	    cv.Optional(CONF_VEML6075_VISIBLE_COMP): sensor.sensor_schema(
                #unit_of_measurement=UNIT_UVINDEX,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
		#icon=CONF_VEML6075_ICON_NUMERIC,
            ),
		
	    cv.Optional(CONF_VEML6075_IR_COMP): sensor.sensor_schema(
                #unit_of_measurement=UNIT_UVINDEX,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
		#icon=CONF_VEML6075_ICON_NUMERIC,
            ),
		
	    cv.Optional(CONF_VEML6075_RAWUVA): sensor.sensor_schema(
                #unit_of_measurement=UNIT_UVA,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
		icon=CONF_VEML6075_ICON_UV,
            ),
		
	    cv.Optional(CONF_VEML6075_RAWUVB): sensor.sensor_schema(
                #unit_of_measurement=UNIT_UVB,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
		icon=CONF_VEML6075_ICON_UV,
            ),	
		
	    cv.Optional(VEML6075_INTEGRATION_TIME_OPTIONS, default="100ms"): cv.enum(VEML6075_INTEGRATION_TIME_OPTIONS),
	    cv.Optional(VEML6075_DYNAMIC_OPTIONS, default="normal"): cv.enum(VEML6075_DYNAMIC_OPTIONS),
	    cv.Optional(VEML6075_AUTOFORCE_OPTIONS, default="disable"): cv.enum(VEML6075_AUTOFORCE_OPTIONS),
        }
    )
    .extend(cv.polling_component_schema(CONF_DEFAULT_POLLING_CONPONENT_SCHEMA))
    .extend(i2c.i2c_device_schema(CONF_DEFAULT_I2C_ADRESS_SCHEMA)),
#     cv.has_at_least_one_key(CONF_UVA, CONF_UVB, CONF_UVINDEX),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_VEML6075_UVA in config:
        sens = await sensor.new_sensor(config[CONF_VEML6075_UVA])
        cg.add(var.set_uva_sensor(sens))
	 
    if CONF_VEML6075_UVB in config:
        sens = await sensor.new_sensor(config[CONF_VEML6075_UVB])
        cg.add(var.set_uva_sensor(sens))
   
    if CONF_VEML6075_UVINDEX in config:
        sens = await sensor.new_sensor(config[CONF_VEML6075_UVINDEX])
        cg.add(var.set_uvindex_sensor(sens))
	
    if CONF_VEML6075_VISIBLE_COMP in config:
        sens = await sensor.new_sensor(config[CONF_VEML6075_VISIBLE_COMP])
        cg.add(var.set_visible_comp_sensor(sens))
	
    if CONF_VEML6075_IR_COMP in config:
        sens = await sensor.new_sensor(config[CONF_VEML6075_IR_COMP])
        cg.add(var.set_ir_comp_sensor(sens))
	
    if CONF_VEML6075_RAWUVA in config:
        sens = await sensor.new_sensor(config[CONF_VEML6075_RAWUVA])
        cg.add(var.set_rawuva_sensor(sens))
	
    if CONF_VEML6075_RAWUVB in config:
        sens = await sensor.new_sensor(config[CONF_VEML6075_RAWUVB])
        cg.add(var.set_rawuvb_sensor(sens))
    
    if VEML6075_INTEGRATION_TIME_OPTIONS in config:
        cg.add(var.set_integration_time(config[VEML6075_INTEGRATION_TIME_OPTIONS]))
    
    if VEM6075_DYNAMIC_OPTIONS in config:
        cg.add(var.set_dynamic(config[VEM6075_DYNAMIC_OPTIONS]))
    
    if VEM6075_AUTOFORCE_OPTIONS in config:
        cg.add(var.set_autoforce(config[VEM6075_AUTOFORCE_OPTIONS]))

	
#     cg.add(var.set_integration_time(config[VEML6075_INTEGRATION_TIME_OPTIONS]))

#     for key, funcName in TYPES.items():
#         if key in config:
#             sens = await sensor.new_sensor(config[key])
#             cg.add(getattr(var, funcName)(sens))
