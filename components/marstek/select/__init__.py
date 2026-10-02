import esphome.codegen as cg
from esphome.components import select
from esphome.components.modbustcp_controller import SENSOR_VALUE_TYPE
import esphome.config_validation as cv

from .. import (
    CONF_MARSTEK_ID,
    CONF_MODEL,
    MARSTEK_PLATFORM_SCHEMA,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    marstek_ns,
    skip_updates,
    union_specs,
)
from ..registers import REGISTERS

DEPENDENCIES = ["marstek"]

MarstekSelect = marstek_ns.class_("MarstekSelect", select.Select, cg.Component)

SPECS = union_specs("select")

CONFIG_SCHEMA = MARSTEK_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): select.select_schema(
            MarstekSelect,
            **entity_kwargs(spec, "icon", "entity_category"),
        )
        for key, spec in SPECS.items()
    }
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("select")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MARSTEK_ID])
    hub_conf = hub_config(config[CONF_MARSTEK_ID])
    specs = REGISTERS[hub_conf[CONF_MODEL]]["select"]

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        # Options in register value order; the n-th option maps to the n-th value
        options = sorted(spec["options"].items(), key=lambda item: item[1])
        var = await select.new_select(conf, options=[name for name, _ in options])
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)

        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                skip_updates(spec, hub_conf),
            )
        )
        for _, value in options:
            cg.add(var.add_value(value))
        cg.add(hub.add_sensor_item(var))
