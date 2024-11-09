import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_HEAT,
)

CONF_SOLARPID_ID = "solarpid_id"
CONF_THERMOSTAT_CUT = "thermostat_cut"

HydreonRGxxBinarySensor = hydreon_rgxx_ns.class_(
    "HydreonRGxxBinaryComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HydreonRGxxBinarySensor),
        cv.GenerateID(CONF_HYDREON_RGXX_ID): cv.use_id(HydreonRGxxComponent),
        cv.Optional(CONF_TOO_COLD): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_COLD
        ),
        cv.Optional(CONF_LENS_BAD): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_EM_SAT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
    }
)
