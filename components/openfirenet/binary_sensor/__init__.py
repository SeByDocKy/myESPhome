import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_OPENFIRENET_ID, OpenFireNetComponent, openfirenet_ns

DEPENDENCIES = ["openfirenet"]

BinarySensorKey = openfirenet_ns.enum("BinarySensorKey")

# key -> (C++ enum name, schema kwargs). Keep in sync with openfirenet.h.
BINARY_SENSORS = {
    "connected": (
        "BINARY_SENSOR_CONNECTED",
        dict(device_class=DEVICE_CLASS_CONNECTIVITY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
    "burning": ("BINARY_SENSOR_BURNING", dict(device_class=DEVICE_CLASS_RUNNING, icon="mdi:fire")),
    "error": ("BINARY_SENSOR_ERROR", dict(device_class=DEVICE_CLASS_PROBLEM)),
    "hopper_lid_open": ("BINARY_SENSOR_HOPPER_LID_OPEN", dict(device_class=DEVICE_CLASS_OPENING)),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENFIRENET_ID): cv.use_id(OpenFireNetComponent),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema(**kwargs)
            for key, (_, kwargs) in BINARY_SENSORS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENFIRENET_ID])
    for key, (enum_name, _) in BINARY_SENSORS.items():
        if conf := config.get(key):
            sens = await binary_sensor.new_binary_sensor(conf)
            cg.add(hub.set_binary_sensor(getattr(BinarySensorKey, enum_name), sens))
