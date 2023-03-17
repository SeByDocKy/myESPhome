mport esphome.codegen as cg
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

CONF_DEFAULT_I2C_ADRESS_SCHEMA = 0x10
CONF_DEFAULT_POLLING_CONPONENT_SCHEMA = "60s"

CONF_VEML6075_UVA      = "uva"
CONF_VEML6075_UVB      = "uvb"
CONF_VEML6075_UVINDEX  = "uvindex"
CONF_VEML6075_UVCOMP1  = "uvcomp1"
CONF_VEML6075_UVCOMP2  = "uvcomp2"
CONF_VEML6075_RAWUVA   = "rawuva"
CONF_VEML6075_RAWUVB   = "rawuva"

CONF_UNIT_UVA          = "#/uW/cm²"
CONF_UNIT_UVB          = "#/uW/cm²"
CONF_UNIT_UVINDEX      = "#"


VEML6075_INTEGRATION_TIME = veml6075_ns.enum("veml6075_uv_it_t")
VEML6075_INTEGRATION_TIME_OPTIONS = {
    "50ms": VEML6075_INTEGRATION_TIME.IT_50MS,
    "100ms": VEML6075_INTEGRATION_TIME.IT_100MS
    "200ms": VEML6075_INTEGRATION_TIME.IT_200MS,
    "400ms": VEML6075_INTEGRATION_TIME.IT_400MS,
    "800ms": VEML6075_INTEGRATION_TIME.IT_800MS,
}

VEM6075_DYNAMIC = veml6075_ns.enum("veml6075_hd_t")
VEM6075_DYNAMIC_OPTIONS = {
   "normal": VEM6075_DYNAMIC.DYNAMIC_NORMAL,
   "high": VEM6075_DYNAMIC.DYNAMIC_HIGH,
}

#VEML6075_REGISTERS = veml6075_ns.enum("VEML6075_REGISTERS")
#VEML6075_CONSTANTS = veml6075_ns.enum("VEML6075_CONSTANTS")


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VEML6075Component),
            cv.Optional(CONF_UV_A): sensor.sensor_schema(
                unit_of_measurement=UNIT_UV_A,
                accuracy_decimals=2,
#                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_UV_B): sensor.sensor_schema(
                unit_of_measurement=UNIT_UV_B,
                accuracy_decimals=2,
#                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
	    cv.Optional(CONF_UV_INDEX): sensor.sensor_schema(
                unit_of_measurement=UNIT_UV_INDEX,
                accuracy_decimals=1,
#                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
	    cv.Optional(VEML6075_INTEGRATION_TIME_OPTIONS, default="100ms"): cv.enum(VEML6075_INTEGRATION_TIME_OPTIONS),
	    cv.Optional(VEML6075_DYNAMIC_OPTIONS, default="normal"): cv.enum(VEML6075_DYNAMIC_OPTIONS),
        }
    )
    .extend(cv.polling_component_schema(CONF_DEFAULT_POLLING_CONPONENT_SCHEMA))
    .extend(i2c.i2c_device_schema(CONF_DEFAULT_I2C_ADRESS_SCHEMA)),
	cv.has_at_least_one_key(CONF_UV_A, CONF_UV_B, CONF_UV_INDEX),
)

TYPES = {
    CONF_UV_A: "set_uv_a",
    CONF_UV_B: "set_uv_b",
	CONF_UV_INDEX: "set_uv_index",
}

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
	
	cg.add(var.set_integration_time(config[VEML6075_INTEGRATION_TIME_OPTIONS]))

    for key, funcName in TYPES.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))
