import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv

from .. import (
    CONF_DEYE_MONO_ID,
    DEYE_MONO_PLATFORM_SCHEMA,
    add_register_item,
    deye_mono_ns,
    entity_kwargs,
)
from ..registers import SELECTS

DEPENDENCIES = ["deye_mono"]

DeyeMonoSelect = deye_mono_ns.class_("DeyeMonoSelect", select.Select, cg.Component)

CONFIG_SCHEMA = DEYE_MONO_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): select.select_schema(DeyeMonoSelect, **entity_kwargs(spec, "icon", "entity_category"))
        for key, spec in SELECTS.items()
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYE_MONO_ID])

    for key, conf in config.items():
        if key not in SELECTS:
            continue
        spec = SELECTS[key]
        # Options in the order of the register table; the n-th option maps to the n-th value
        var = await select.new_select(conf, options=[name for name, _ in spec["options"]])
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_mask(spec["mask"]))
        for _, value in spec["options"]:
            cg.add(var.add_value(value))
        cg.add(hub.track_register(spec["register"]))
        add_register_item(hub, var, spec)
