"""Switch platform (shared by deyehylv_fast and deyehylv_slow)."""

import esphome.codegen as cg
from esphome.components import switch
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

DeyeHyLvSwitch = deyehylv_ns.class_("DeyeHyLvSwitch", switch.Switch, cg.Component)

ENTITY_SCHEMA = DEYEHYLV_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): switch.switch_schema(DeyeHyLvSwitch, **entity_kwargs(spec, "icon", "entity_category"))
        for key, spec in union_specs("switch").items()
    }
)
ENTITY_FINAL_VALIDATE_SCHEMA = final_validate_platform("switch")


# `platform: deyehylv` is never written in a YAML: the speed platforms (deyehylv_fast / deyehylv_slow) auto-load it so
# that the C++ of this platform is built once, whichever speed(s) are used. It carries no entity itself.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass


async def build_entities(config, slow):
    hub = await cg.get_variable(config[CONF_DEYEHYLV_ID])
    specs, _ = hub_phase_specs(config[CONF_DEYEHYLV_ID], "switch")

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_mask(spec["mask"]))
        cg.add(hub.track_register(spec["register"]))
        add_register_item(hub, var, spec, slow)
