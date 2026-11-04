"""Number platform (shared by deyelvhy_fast and deyelvhy_slow)."""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MODE

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

DeyeLvHyNumber = deyelvhy_ns.class_("DeyeLvHyNumber", number.Number, cg.Component)

ENTITY_SCHEMA = DEYELVHY_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): number.number_schema(
            DeyeLvHyNumber, **entity_kwargs(spec, "unit_of_measurement", "icon", "entity_category")
        )
        for key, spec in union_specs("number").items()
    }
)
ENTITY_FINAL_VALIDATE_SCHEMA = final_validate_platform("number")


# `platform: deyelvhy` is never written in a YAML: the speed platforms (deyelvhy_fast / deyelvhy_slow) auto-load it so
# that the C++ of this platform is built once, whichever speed(s) are used. It carries no entity itself.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass


async def build_entities(config, slow):
    hub = await cg.get_variable(config[CONF_DEYELVHY_ID])
    specs, _ = hub_phase_specs(config[CONF_DEYELVHY_ID], "number")

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await number.new_number(
            conf,
            min_value=spec["min"],
            max_value=spec["max"],
            step=spec["step"],
        )
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        # slider / box of the register table, unless the YAML chose a mode itself
        if "mode" in spec and conf[CONF_MODE] == number.DEFAULT_MODE:
            cg.add(var.traits.set_mode(number.NUMBER_MODES[spec["mode"].upper()]))
        cg.add(var.set_scale(spec.get("scale", 1.0)))
        cg.add(hub.track_register(spec["register"]))
        add_register_item(hub, var, spec, slow)
