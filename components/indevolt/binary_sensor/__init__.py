import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_INDEVOLT_ID, IndevoltComponent, both, g2, hub_generation, make_final_validate

DEPENDENCIES = ["indevolt"]

CONF_ONLINE = "online"

# (config key, points per generation, raw value meaning "on", schema kwargs)
BINARY_SENSORS = [
    # 7120: 1000 = meter connected ("ON" in the official integration), 1001 = not connected.
    ("meter_connected", both(7120), 1000, dict(device_class=DEVICE_CLASS_CONNECTIVITY, icon="mdi:meter-electric")),
    # 680: bypass active (1). Gen2 only; the `bypass` switch reads the same point.
    ("bypass_active", g2(680), 1, dict(icon="mdi:electric-switch")),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INDEVOLT_ID): cv.use_id(IndevoltComponent),
        # Not a device point: reflects whether polling the device succeeds.
        cv.Optional(CONF_ONLINE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:lan-connect",
        ),
        **{cv.Optional(key): binary_sensor.binary_sensor_schema(**kwargs) for key, _p, _on, kwargs in BINARY_SENSORS},
    }
)


def _entries(config):
    for key, points, on_value, _kwargs in BINARY_SENSORS:
        if key in config:
            yield key, points, config[key], on_value


FINAL_VALIDATE_SCHEMA = make_final_validate(_entries)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_INDEVOLT_ID])
    gen = hub_generation(config)
    if CONF_ONLINE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_ONLINE])
        cg.add(hub.set_online_binary_sensor(s))
    for _label, points, conf, on_value in _entries(config):
        s = await binary_sensor.new_binary_sensor(conf)
        cg.add(hub.add_binary_sensor(points[gen], on_value, s))
