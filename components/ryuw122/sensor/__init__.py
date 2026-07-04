import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_DISTANCE,
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CENTIMETER,
    UNIT_DECIBEL_MILLIWATT,
    ICON_SIGNAL,
)

from .. import ryuw122_ns, RYUW122Component

DEPENDENCIES = ["ryuw122"]

CONF_RYUW122_ID = "ryuw122_id"
CONF_RSSI = "rssi"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RYUW122_ID): cv.use_id(RYUW122Component),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CENTIMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            icon=ICON_SIGNAL,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RYUW122_ID])

    if CONF_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DISTANCE])
        cg.add(parent.set_distance_sensor(sens))

    if CONF_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_RSSI])
        cg.add(parent.set_rssi_sensor(sens))
