import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_HEAT,
)
# from . import solarpid_ns

DEPENDENCIES = ["solarpid"]
CONF_SOLARPID_ID = "solarpid_id"
CONF_THERMOSTAT_CUT = "thermostat_cut"


SOLARPIDBinarySensor = solarpid_ns.class_(
    "SOLARPIDBinarySensor", binary_sensor.Binary_Sensor, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SOLARPIDBinarySensor),
        cv.GenerateID(CONF_SOLARPID_ID): cv.use_id(SOLARPID),
        cv.Optional(CONF_THERMOSTAT_CUT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_HEAT
        ),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_THERMOSTAT_CUT in config:
      bin_sens = await cg.get_variable(config[CONF_THERMOSTAT_CUT])
      cg.add(var.set_thermostat_cut_binary_sensor(bin_sens))
