"""Select platform (shared by deyelvhy_fast and deyelvhy_slow)."""

import esphome.codegen as cg
from esphome.components import select
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

DeyeLvHySelect = deyelvhy_ns.class_("DeyeLvHySelect", select.Select, cg.Component)

ENTITY_SCHEMA = DEYELVHY_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): select.select_schema(DeyeLvHySelect, **entity_kwargs(spec, "icon", "entity_category"))
        for key, spec in union_specs("select").items()
    }
)
ENTITY_FINAL_VALIDATE_SCHEMA = final_validate_platform("select")


# `platform: deyelvhy` is never written in a YAML: the speed platforms (deyelvhy_fast / deyelvhy_slow) auto-load it so
# that the C++ of this platform is built once, whichever speed(s) are used. It carries no entity itself.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass


async def build_entities(config, slow):
    hub = await cg.get_variable(config[CONF_DEYELVHY_ID])
    specs, _ = hub_phase_specs(config[CONF_DEYELVHY_ID], "select")

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
