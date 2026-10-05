import esphome.codegen as cg
from esphome.components import select
from esphome.components.modbustcp_controller import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE
import esphome.config_validation as cv

from .. import (
    CONF_ANKER_SOLIX_ID,
    CONF_MODEL,
    ANKER_SOLIX_PLATFORM_SCHEMA,
    anker_solix_ns,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    skip_updates,
    union_specs,
)
from ..registers import REGISTERS

DEPENDENCIES = ["anker_solix"]

AnkerSelect = anker_solix_ns.class_("AnkerSelect", select.Select, cg.Component)

SPECS = union_specs("select")

CONFIG_SCHEMA = ANKER_SOLIX_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): select.select_schema(
            AnkerSelect, **entity_kwargs(spec, "icon", "entity_category")
        )
        for key, spec in SPECS.items()
    }
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("select")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ANKER_SOLIX_ID])
    hub_conf = hub_config(config[CONF_ANKER_SOLIX_ID])
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
                MODBUS_REGISTER_TYPE[spec["rtype"]],
                skip_updates(spec, hub_conf),
            )
        )
        for _, value in options:
            cg.add(var.add_value(value, spec["option_bits"][value]))
        # Needs the EMS mode mask to refuse the modes the device does not support
        cg.add(hub.enable_mode_tracking())
        cg.add(hub.add_sensor_item(var))
