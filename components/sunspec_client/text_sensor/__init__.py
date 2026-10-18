import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_SUNSPEC_CLIENT_ID, SunSpecClient

DEPENDENCIES = ["sunspec_client"]

CONF_MODELS = "models"

ICON_MODELS = "mdi:format-list-numbered"

# `models`: ids of the SunSpec models exposed by the connected device, found by the scan the
# component runs at startup (for example "1, 103, 160"). It reads "scanning" while the scan runs
# and "scan failed" when it failed (a new attempt follows after scan_retry_interval).
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SUNSPEC_CLIENT_ID): cv.use_id(SunSpecClient),
        cv.Optional(CONF_MODELS): text_sensor.text_sensor_schema(
            icon=ICON_MODELS, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)

_SETTERS = {
    CONF_MODELS: "set_models_text_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SUNSPEC_CLIENT_ID])
    for key, setter in _SETTERS.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(hub, setter)(sens))
