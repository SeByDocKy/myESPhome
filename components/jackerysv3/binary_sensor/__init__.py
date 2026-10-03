import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_JACKERYSV3_ID, JackerySV3Hub

DEPENDENCIES = ["jackerysv3"]

# (config key, decoder key, schema attributes). The decoder keys are known by JackerySV3Hub::add_binary_sensor().
BINARY_SENSORS = [
    # Hub level: the battery reports regularly / at least one MQTT client is connected to the embedded broker
    ("online", "online", dict(device_class=DEVICE_CLASS_CONNECTIVITY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
    (
        "client_connected",
        "client_connected",
        dict(device_class=DEVICE_CLASS_CONNECTIVITY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:lan-connect"),
    ),
    # Reported by the battery (ongridStat, ctStat, gridSate, swEpsState)
    ("on_grid", "on_grid", dict(icon="mdi:transmission-tower")),
    ("ct_online", "ct_online", dict(icon="mdi:current-ac", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
    ("grid_meter_link", "grid_meter_link", dict(icon="mdi:lan-connect", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
    ("socket_ok", "socket_ok", dict(icon="mdi:power-socket-eu", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_JACKERYSV3_ID): cv.use_id(JackerySV3Hub),
        **{cv.Optional(key): binary_sensor.binary_sensor_schema(**attrs) for key, _flat, attrs in BINARY_SENSORS},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JACKERYSV3_ID])
    for key, flat, _attrs in BINARY_SENSORS:
        if key in config:
            var = await binary_sensor.new_binary_sensor(config[key])
            cg.add(hub.add_binary_sensor(flat, var))
