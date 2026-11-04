"""Text sensor platform (shared by deyehylv_fast and deyehylv_slow)."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import (
    CONF_DEYEHYLV_ID,
    DEYEHYLV_PLATFORM_SCHEMA,
    add_register_item,
    deyehylv_ns,
    entity_kwargs,
    final_validate_platform,
    hub_phase_specs,
    union_specs,
)

DeyeHyLvStateTextSensor = deyehylv_ns.class_("DeyeHyLvStateTextSensor", text_sensor.TextSensor, cg.Component)
DeyeHyLvTimeTextSensor = deyehylv_ns.class_("DeyeHyLvTimeTextSensor", text_sensor.TextSensor, cg.Component)

CLASSES = {"state": DeyeHyLvStateTextSensor, "time": DeyeHyLvTimeTextSensor}

CONFIG_SCHEMA = DEYEHYLV_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): text_sensor.text_sensor_schema(
            CLASSES[spec["kind"]], **entity_kwargs(spec, "icon", "entity_category")
        )
        for key, spec in union_specs("text_sensor").items()
    }
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("text_sensor")


async def to_code(config, slow):
    hub = await cg.get_variable(config[CONF_DEYEHYLV_ID])
    specs, _ = hub_phase_specs(config[CONF_DEYEHYLV_ID], "text_sensor")

    for key, conf in config.items():
        if key not in specs:
            continue
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)
        add_register_item(hub, var, specs[key], slow)
