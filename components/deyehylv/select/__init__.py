"""Select platform (shared by deyehylv_fast and deyehylv_slow)."""

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv

from esphome.components.deyehylv import (
    CONF_DEYEHYLV_ID,
    DEYEHYLV_PLATFORM_SCHEMA,
    add_register_item,
    deyehylv_ns,
    entity_kwargs,
    final_validate_platform,
    hub_phase_specs,
    union_specs,
)

DeyeHyLvSelect = deyehylv_ns.class_("DeyeHyLvSelect", select.Select, cg.Component)

ENTITY_SCHEMA = DEYEHYLV_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): select.select_schema(DeyeHyLvSelect, **entity_kwargs(spec, "icon", "entity_category"))
        for key, spec in union_specs("select").items()
    }
)
ENTITY_FINAL_VALIDATE_SCHEMA = final_validate_platform("select")


# `platform: deyehylv` is never written in a YAML: the speed platforms (deyehylv_fast / deyehylv_slow) auto-load it so
# that the C++ of this platform is built once, whichever speed(s) are used. It carries no entity itself.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass


async def build_entities(config, slow):
    hub = await cg.get_variable(config[CONF_DEYEHYLV_ID])
    specs, _ = hub_phase_specs(config[CONF_DEYEHYLV_ID], "select")

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        # Options in the order of the register table; the n-th option maps to the n-th value
        var = await select.new_select(conf, options=[name for name, _ in spec["options"]])
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_mask(spec["mask"]))
        for _, value in spec["options"]:
            cg.add(var.add_value(value))
        cg.add(hub.track_register(spec["register"]))
        add_register_item(hub, var, spec, slow)
