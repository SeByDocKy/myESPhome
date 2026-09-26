import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_ZENSDK_ID, ZenSdkComponent

DEPENDENCIES = ["zensdk"]

CONF_ONLINE = "online"

# (config key, zenSDK property, schema kwargs). Values are published as `value != 0`.
BINARY_SENSORS = [
    ("heat_state", "heatState", dict(device_class=DEVICE_CLASS_HEAT, icon="mdi:heat-wave")),
    ("bypass", "pass", dict(icon="mdi:electric-switch")),
    ("reverse_state", "reverseState", dict(icon="mdi:swap-horizontal")),
    ("grid_connected", "gridState", dict(device_class=DEVICE_CLASS_CONNECTIVITY, icon="mdi:transmission-tower")),
    ("pv_active", "pvStatus", dict(icon="mdi:solar-panel")),
    ("soc_calibrating", "socStatus", dict(icon="mdi:battery-sync")),
    ("error", "is_error", dict(device_class=DEVICE_CLASS_PROBLEM, icon="mdi:alert-circle")),
    ("fan", "fanSwitch", dict(icon="mdi:fan")),
    ("lamp", "lampSwitch", dict(icon="mdi:lightbulb")),
    ("data_ready", "dataReady", dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:database-check")),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        # Not a device property: reflects whether polling the device succeeds.
        cv.Optional(CONF_ONLINE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:lan-connect",
        ),
        **{cv.Optional(key): binary_sensor.binary_sensor_schema(**kwargs) for key, _prop, kwargs in BINARY_SENSORS},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])
    if CONF_ONLINE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_ONLINE])
        cg.add(hub.set_online_binary_sensor(s))
    for key, prop, _kwargs in BINARY_SENSORS:
        if key in config:
            s = await binary_sensor.new_binary_sensor(config[key])
            cg.add(hub.add_binary_sensor(prop, s))
