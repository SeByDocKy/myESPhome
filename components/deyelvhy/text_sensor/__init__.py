"""Text sensor platform (shared by deyelvhy_fast and deyelvhy_slow)."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from esphome.components.deyelvhy import (
    CONF_DEYELVHY_ID,
    DEYELVHY_PLATFORM_SCHEMA,
    add_register_item,
    deyelvhy_ns,
    entity_kwargs,
    final_validate_platform,
    hub_phase_specs,
    union_specs,
)

DeyeLvHyStateTextSensor = deyelvhy_ns.class_("DeyeLvHyStateTextSensor", text_sensor.TextSensor, cg.Component)
DeyeLvHyTimeTextSensor = deyelvhy_ns.class_("DeyeLvHyTimeTextSensor", text_sensor.TextSensor, cg.Component)

CLASSES = {"state": DeyeLvHyStateTextSensor, "time": DeyeLvHyTimeTextSensor}

ENTITY_SCHEMA = DEYELVHY_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): text_sensor.text_sensor_schema(
            CLASSES[spec["kind"]], **entity_kwargs(spec, "icon", "entity_category")
        )
        for key, spec in union_specs("text_sensor").items()
    }
)
ENTITY_FINAL_VALIDATE_SCHEMA = final_validate_platform("text_sensor")


# `platform: deyelvhy` is never written in a YAML: the speed platforms (deyelvhy_fast / deyelvhy_slow) auto-load it so
# that the C++ of this platform is built once, whichever speed(s) are used. It carries no entity itself.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass


async def build_entities(config, slow):
    hub = await cg.get_variable(config[CONF_DEYELVHY_ID])
    specs, _ = hub_phase_specs(config[CONF_DEYELVHY_ID], "text_sensor")

    for key, conf in config.items():
        if key not in specs:
            continue
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)
        add_register_item(hub, var, specs[key], slow)
