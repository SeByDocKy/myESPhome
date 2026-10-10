import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import (
    CONF_DEYE_MONO_ID,
    DEYE_MONO_PLATFORM_SCHEMA,
    add_register_item,
    deye_mono_ns,
    entity_kwargs,
)
from ..registers import SWITCHES

DEPENDENCIES = ["deye_mono"]

DeyeMonoSwitch = deye_mono_ns.class_("DeyeMonoSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = DEYE_MONO_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): switch.switch_schema(DeyeMonoSwitch, **entity_kwargs(spec, "icon", "entity_category"))
        for key, spec in SWITCHES.items()
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYE_MONO_ID])

    for key, conf in config.items():
        if key not in SWITCHES:
            continue
        spec = SWITCHES[key]
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_mask(spec["mask"]))
        cg.add(hub.track_register(spec["register"]))
        add_register_item(hub, var, spec)
