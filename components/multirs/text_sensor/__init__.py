import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_MULTIRS_ID, MultiRS, multirs_ns

DEPENDENCIES = ["multirs"]

MultiRSTextSensor = multirs_ns.class_("MultiRSTextSensor", text_sensor.TextSensor, cg.Parented.template(MultiRS))

# key -> (TextSensorKind value, schema). The numeric kinds mirror the C++ enum TextSensorKind in multirs.h.
TEXT_SENSORS = {
    "state": (0, dict(icon="mdi:state-machine")),
    "error": (1, dict(icon="mdi:alert-circle-outline")),
    "firmware_version": (2, dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:chip")),
    "model": (3, dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:information-outline")),
    "serial_number": (4, dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:identifier")),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MULTIRS_ID): cv.use_id(MultiRS),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(MultiRSTextSensor, **kwargs)
            for key, (_, kwargs) in TEXT_SENSORS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MULTIRS_ID])
    for key, (kind, _) in TEXT_SENSORS.items():
        if key in config:
            var = await text_sensor.new_text_sensor(config[key])
            await cg.register_parented(var, config[CONF_MULTIRS_ID])
            cg.add(var.set_kind(kind))
            cg.add(hub.register_text_sensor(var))
