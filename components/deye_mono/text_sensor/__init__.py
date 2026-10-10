import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import (
    CONF_DEYE_MONO_ID,
    DEYE_MONO_PLATFORM_SCHEMA,
    add_register_item,
    deye_mono_ns,
    entity_kwargs,
)
from ..registers import TEXT_SENSORS

DEPENDENCIES = ["deye_mono"]

DeyeMonoStateTextSensor = deye_mono_ns.class_(
    "DeyeMonoStateTextSensor", text_sensor.TextSensor, cg.Component
)
DeyeMonoTimeTextSensor = deye_mono_ns.class_(
    "DeyeMonoTimeTextSensor", text_sensor.TextSensor, cg.Component
)

CLASSES = {"state": DeyeMonoStateTextSensor, "time": DeyeMonoTimeTextSensor}

CONFIG_SCHEMA = DEYE_MONO_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): text_sensor.text_sensor_schema(
            CLASSES[spec["kind"]], **entity_kwargs(spec, "icon", "entity_category")
        )
        for key, spec in TEXT_SENSORS.items()
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYE_MONO_ID])

    for key, conf in config.items():
        if key not in TEXT_SENSORS:
            continue
        spec = TEXT_SENSORS[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)
        add_register_item(hub, var, spec)
