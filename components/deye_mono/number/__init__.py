import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MODE

from .. import (
    CONF_DEYE_MONO_ID,
    DEYE_MONO_PLATFORM_SCHEMA,
    add_register_item,
    deye_mono_ns,
    entity_kwargs,
)
from ..registers import NUMBERS

DEPENDENCIES = ["deye_mono"]

DeyeMonoNumber = deye_mono_ns.class_("DeyeMonoNumber", number.Number, cg.Component)

CONFIG_SCHEMA = DEYE_MONO_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): number.number_schema(
            DeyeMonoNumber, **entity_kwargs(spec, "unit_of_measurement", "icon", "entity_category")
        )
        for key, spec in NUMBERS.items()
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYE_MONO_ID])

    for key, conf in config.items():
        if key not in NUMBERS:
            continue
        spec = NUMBERS[key]
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
        add_register_item(hub, var, spec)
