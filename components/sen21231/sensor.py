import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
	CONF_ADDRESS,
	CONF_DEBUG,
	STATE_CLASS_MEASUREMENT,
	ICON_MOTION_SENSOR,
	ICON_RULER,
)

CODEOWNERS                = ["@shreyaskarnik"]
DEPENDENCIES              = ["i2c"]

CONF_SEN21231_NFACES      = "nfaces"
CONF_SEN21231_BOXCONF0    = "boxconf0"
CONF_SEN21231_X0          = "x0"
CONF_SEN21231_Y0          = "y0"
CONF_SEN21231_W0          = "w0"
CONF_SEN21231_H0          = "h0"
CONF_SEN21231_IDCONF0     = "idconf0"
CONF_SEN21231_ID0         = "id0"
CONF_SEN21231_ISFACING0   = "isfacing0"
CONF_SEN21231_DEBUG       = "debug"
CONF_SEN21231_ICON_BOX    = "mdi:face-man"
CONF_SEN21231_ICON_PERCENT= "mdi:percent"
CONF_SEN21231_ICON_TARGET = "mdi:target-variant"


sen21231_ns               = cg.esphome_ns.namespace("sen21231")

SEN21231Component         = sen21231_ns.class_(
    "SEN21231Component", cg.PollingComponent, i2c.I2CDevice
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SEN21231Component),
			 cv.Optional(CONF_ADDRESS): cv.i2c_address,
                         cv.Optional(CONF_SEN21231_NFACES): sensor.sensor_schema(
                                icon=CONF_SEN21231_ICON_BOX,
                                accuracy_decimals=0,
			        state_class=STATE_CLASS_MEASUREMENT,
            ),
                         cv.Optional(CONF_SEN21231_BOXCONF0): sensor.sensor_schema(
                                icon=CONF_SEN21231_ICON_PERCENT,
				accuracy_decimals=0,
				state_class=STATE_CLASS_MEASUREMENT,
            ),
			 cv.Optional(CONF_SEN21231_X0): sensor.sensor_schema(
			        icon=CONF_SEN21231_ICON_TARGET,
                                accuracy_decimals=0,
				state_class=STATE_CLASS_MEASUREMENT,
            ),
			 cv.Optional(CONF_SEN21231_Y0): sensor.sensor_schema(
			        icon=CONF_SEN21231_ICON_TARGET,
                                accuracy_decimals=0,
				state_class=STATE_CLASS_MEASUREMENT,
            ),
			 cv.Optional(CONF_SEN21231_W0): sensor.sensor_schema(
			        icon=CONF_SEN21231_ICON_TARGET,
                                accuracy_decimals=0,
				state_class=STATE_CLASS_MEASUREMENT,
            ),
			cv.Optional(CONF_SEN21231_H0): sensor.sensor_schema(
			        icon=CONF_SEN21231_ICON_TARGET,
                                accuracy_decimals=0,
				state_class=STATE_CLASS_MEASUREMENT,
            ),
	                cv.Optional(CONF_SEN21231_IDCONF0): sensor.sensor_schema(
                                icon=CONF_SEN21231_ICON_PERCENT,
				accuracy_decimals=0,
				state_class=STATE_CLASS_MEASUREMENT,
            ),	
            	        cv.Optional(CONF_SEN21231_ID0): sensor.sensor_schema(
                                icon=CONF_SEN21231_ICON_TARGET,
				accuracy_decimals=0,
				state_class=STATE_CLASS_MEASUREMENT,
            ),	
			cv.Optional(CONF_SEN21231_ISFACING0): sensor.sensor_schema(
                                accuracy_decimals=0,
				state_class=STATE_CLASS_MEASUREMENT,
				icon=CONF_SEN21231_ICON_BOX,
            ),    
#		        cv.Optional(CONF_SEN21231_DEBUG , default="true"): cv.enum(
#                                    SEN21231_DEBUG_OPTIONS
#	    ),	
			cv.Optional(CONF_DEBUG, default=True): cv.boolean,	       
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x62))
#	cv.has_at_least_one_key(CONF_SEN21231_NFACES),
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
	
    if CONF_SEN21231_NFACES in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_NFACES])
        cg.add(var.set_nfaces_sensor(sens))
	
    if CONF_SEN21231_BOXCONF0 in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_BOXCONF0])
        cg.add(var.set_boxconf0_sensor(sens))
		
    if CONF_SEN21231_X0 in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_X0])
        cg.add(var.set_x0_sensor(sens))
		
    if CONF_SEN21231_Y0 in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_Y0])
        cg.add(var.set_y0_sensor(sens))
		
    if CONF_SEN21231_W0 in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_W0])
        cg.add(var.set_w0_sensor(sens))
		
    if CONF_SEN21231_H0 in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_H0])
        cg.add(var.set_h0_sensor(sens))
	
    if CONF_SEN21231_IDCONF0 in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_IDCONF0])
        cg.add(var.set_idconf0_sensor(sens))
	
    if CONF_SEN21231_ID0 in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_ID0])
        cg.add(var.set_id0_sensor(sens))	
		
    if CONF_SEN21231_ISFACING0 in config:
        sens = await sensor.new_sensor(config[CONF_SEN21231_ISFACING0])
        cg.add(var.set_isfacing0_sensor(sens))
    
#    if CONF_DEBUG in config:
#        cg.add(var.set_debug(conf[CONF_DEBUG]))
	
