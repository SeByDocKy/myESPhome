import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_JACKERYSV3_ID, JackerySV3Hub

DEPENDENCIES = ["jackerysv3"]

# (config key, decoder key, schema attributes)
TEXT_SENSORS = [
    # Normal / Waiting / Alarm / Fault / Standby / Low power ("Offline" while the battery is silent)
    ("status", "status", dict(icon="mdi:state-machine")),
    ("work_mode", "work_mode", dict(icon="mdi:cog-outline")),
    ("firmware_version", "firmware_version", dict(icon="mdi:chip", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
    ("model", "model", dict(icon="mdi:information-outline", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
    ("ct_type", "ct_type", dict(icon="mdi:current-ac", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_JACKERYSV3_ID): cv.use_id(JackerySV3Hub),
        **{cv.Optional(key): text_sensor.text_sensor_schema(**attrs) for key, _flat, attrs in TEXT_SENSORS},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JACKERYSV3_ID])
    for key, flat, _attrs in TEXT_SENSORS:
        if key in config:
            var = await text_sensor.new_text_sensor(config[key])
            cg.add(hub.add_text_sensor(flat, 0, var))
