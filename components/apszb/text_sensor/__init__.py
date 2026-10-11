import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import APSInverter, CONF_INVERTER, CONF_SERIAL, CONF_SHORT_ADDRESS

DEPENDENCIES = ["apszb"]

CONF_STATUS = "status"

TEXT_SENSORS = {
    CONF_STATUS: (
        text_sensor.text_sensor_schema(icon="mdi:information-outline"),
        "set_status_text_sensor",
    ),
    CONF_SERIAL: (
        text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:identifier"
        ),
        "set_serial_text_sensor",
    ),
    CONF_SHORT_ADDRESS: (
        text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:lan"
        ),
        "set_short_address_text_sensor",
    ),
}

_schema = {cv.Required(CONF_INVERTER): cv.use_id(APSInverter)}
for _key, (_sch, _setter) in TEXT_SENSORS.items():
    _schema[cv.Optional(_key)] = _sch

CONFIG_SCHEMA = cv.Schema(_schema)


async def to_code(config):
    inv = await cg.get_variable(config[CONF_INVERTER])
    for key, (_, setter) in TEXT_SENSORS.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(inv, setter)(sens))
