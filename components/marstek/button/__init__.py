import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv

from .. import (
    CONF_MARSTEK_ID,
    CONF_MODEL,
    MARSTEK_PLATFORM_SCHEMA,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    marstek_ns,
    union_specs,
)
from ..registers import REGISTERS

DEPENDENCIES = ["marstek"]

MarstekButton = marstek_ns.class_("MarstekButton", button.Button)

SPECS = union_specs("button")

CONFIG_SCHEMA = MARSTEK_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): button.button_schema(
            MarstekButton,
            **entity_kwargs(spec, "icon", "entity_category"),
        )
        for key, spec in SPECS.items()
    }
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("button")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MARSTEK_ID])
    hub_conf = hub_config(config[CONF_MARSTEK_ID])
    specs = REGISTERS[hub_conf[CONF_MODEL]]["button"]

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await button.new_button(conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_command(spec["register"], spec["command"]))
