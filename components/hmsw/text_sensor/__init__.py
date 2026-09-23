import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from .. import HMSWComponent

CONF_HMSW_ID = "hmsw_id"
# RealDataNew (0xA3 0x11) only -- never published when data_source:
# real_data (the default) is in use. See README.md.
CONF_FIRMWARE_VERSION = "firmware_version"

DEPENDENCIES = ["hmsw"]

_FIRMWARE_VERSION_SCHEMA = text_sensor.text_sensor_schema(
    entity_category="diagnostic",
    icon="mdi:chip",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMSW_ID): cv.use_id(HMSWComponent),
        cv.Optional(CONF_FIRMWARE_VERSION): _FIRMWARE_VERSION_SCHEMA,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMSW_ID])

    if CONF_FIRMWARE_VERSION in config:
        s = await text_sensor.new_text_sensor(config[CONF_FIRMWARE_VERSION])
        cg.add(hub.set_firmware_version_sensor(s))
