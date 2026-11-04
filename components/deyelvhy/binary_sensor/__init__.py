"""Binary sensor platform (shared by deyelvhy_fast and deyelvhy_slow)."""

import esphome.codegen as cg
from esphome.components import binary_sensor
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

DeyeLvHyBinarySensor = deyelvhy_ns.class_("DeyeLvHyBinarySensor", binary_sensor.BinarySensor, cg.Component)

ENTITY_SCHEMA = DEYELVHY_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): binary_sensor.binary_sensor_schema(
            DeyeLvHyBinarySensor, **entity_kwargs(spec, "icon", "entity_category", "device_class")
        )
        for key, spec in union_specs("binary_sensor").items()
    }
)
ENTITY_FINAL_VALIDATE_SCHEMA = final_validate_platform("binary_sensor")


# `platform: deyelvhy` is never written in a YAML: the speed platforms (deyelvhy_fast / deyelvhy_slow) auto-load it so
# that the C++ of this platform is built once, whichever speed(s) are used. It carries no entity itself.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass


async def build_entities(config, slow):
    hub = await cg.get_variable(config[CONF_DEYELVHY_ID])
    specs, _ = hub_phase_specs(config[CONF_DEYELVHY_ID], "binary_sensor")

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await binary_sensor.new_binary_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_mask(spec["mask"]))
        add_register_item(hub, var, spec, slow)
