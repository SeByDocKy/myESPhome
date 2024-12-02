import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from esphome.const import (
    DEVICE_CLASS_OCCUPANCY,
)

from .. import CONF_OFFSR_ID, OFFSRComponent

DEPENDENCIES = ["offsr"]

CONF_THERMOSTAT_CUT = "thermostat_cut"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
    cv.Optional(CONF_THERMOSTAT_CUT): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
    ),
}

async def to_code(config):
    offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    if thermostat_cut_config := config.get(CONF_THERMOSTAT_CUT):
        sens = await binary_sensor.new_binary_sensor(thermostat_cut_config)
        cg.add(offsr_component.set_thermostat_cut_binary_sensor(sens))
