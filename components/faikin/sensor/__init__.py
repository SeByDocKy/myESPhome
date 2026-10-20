import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ICON_FAN,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_REVOLUTIONS_PER_MINUTE,
)

from .. import CONF_FAIKIN_ID, FaikinComponent

DEPENDENCIES = ["faikin"]

CONF_HOME_TEMPERATURE = "home_temperature"
CONF_OUTSIDE_TEMPERATURE = "outside_temperature"
CONF_LIQUID_TEMPERATURE = "liquid_temperature"
CONF_FAN_SPEED = "fan_speed"
CONF_COMPRESSOR = "compressor"


def _temperature_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


SENSORS = {
    # Room temperature measured by the indoor unit.
    CONF_HOME_TEMPERATURE: _temperature_schema(),
    CONF_OUTSIDE_TEMPERATURE: _temperature_schema(),
    # Liquid / coil temperature (command RI, exact meaning not fully confirmed).
    CONF_LIQUID_TEMPERATURE: _temperature_schema(),
    CONF_FAN_SPEED: sensor.sensor_schema(
        unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        accuracy_decimals=0,
        icon=ICON_FAN,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # Raw compressor value (command Rd), no unit confirmed.
    CONF_COMPRESSOR: sensor.sensor_schema(
        accuracy_decimals=0,
        icon="mdi:engine",
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FAIKIN_ID): cv.use_id(FaikinComponent),
        **{cv.Optional(key): schema for key, schema in SENSORS.items()},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FAIKIN_ID])
    for key in SENSORS:
        if conf := config.get(key):
            var = await sensor.new_sensor(conf)
            cg.add(getattr(parent, f"set_{key}_sensor")(var))
