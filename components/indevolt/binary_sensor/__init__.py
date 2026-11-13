import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_CONNECTIVITY, DEVICE_CLASS_HEAT, ENTITY_CATEGORY_DIAGNOSTIC

from .. import (
    CONF_INDEVOLT_ID,
    IndevoltComponent,
    apply_poll,
    both,
    g1,
    g2,
    hub_generation,
    make_final_validate,
    with_poll,
)

DEPENDENCIES = ["indevolt"]

CONF_ONLINE = "online"

# (config key, points per generation, raw value meaning "on", schema kwargs)
BINARY_SENSORS = [
    # 7120: 1000 = meter connected ("ON" in the official integration), 1001 = not connected.
    ("meter_connected", both(7120), 1000, dict(device_class=DEVICE_CLASS_CONNECTIVITY, icon="mdi:meter-electric")),
    # 680: bypass active (1). Gen2 only; the `bypass` switch reads the same point.
    ("bypass_active", g2(680), 1, dict(icon="mdi:electric-switch")),
    # Electric heating: 1000 = heating on in the official integration (Gen1: 7121 for the unit, Gen2: one point per pack).
    ("heating", g1(7121), 1000, dict(device_class=DEVICE_CLASS_HEAT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:radiator")),
    ("mb_heating", g2(9080), 1000, dict(device_class=DEVICE_CLASS_HEAT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:radiator")),
    ("pack1_heating", g2(9096), 1000, dict(device_class=DEVICE_CLASS_HEAT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:radiator")),
    ("pack2_heating", g2(9112), 1000, dict(device_class=DEVICE_CLASS_HEAT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:radiator")),
    ("pack3_heating", g2(9128), 1000, dict(device_class=DEVICE_CLASS_HEAT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:radiator")),
    ("pack4_heating", g2(9144), 1000, dict(device_class=DEVICE_CLASS_HEAT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:radiator")),
    ("pack5_heating", g2(9279), 1000, dict(device_class=DEVICE_CLASS_HEAT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:radiator")),
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
        **{cv.Optional(key): with_poll(binary_sensor.binary_sensor_schema(**kwargs)) for key, _p, _on, kwargs in BINARY_SENSORS},
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
        apply_poll(hub, points[gen], conf)
