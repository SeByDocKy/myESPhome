import esphome.codegen as cg
from esphome.components import number
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE

from .. import (
    BLUETTI_PLATFORM_SCHEMA,
    CONF_BLUETTI_ID,
    CONF_MODEL,
    bluetti_ns,
    build_tree,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    is_slow,
    tree_schema,
    union_specs,
    walk_config,
)
from ..registers import REGISTERS

DEPENDENCIES = ["bluetti"]

BluettiNumber = bluetti_ns.class_("BluettiNumber", number.Number, cg.Component)

# battery.soc_low_limit / battery.soc_high_limit (5..100 %)
SPECS = union_specs("number")
TREE = build_tree(SPECS)


def _schema(key):
    return number.number_schema(
        BluettiNumber, **entity_kwargs(SPECS[key], "unit_of_measurement", "icon", "entity_category")
    )


CONFIG_SCHEMA = BLUETTI_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("number", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BLUETTI_ID])
    hub_conf = hub_config(config[CONF_BLUETTI_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["number"]

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        spec = available[key]
        var = await number.new_number(conf, min_value=spec["min"], max_value=spec["max"], step=spec["step"])
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                MODBUS_REGISTER_TYPE["holding"],
            )
        )
        cg.add(hub.add_item(var, is_slow(spec)))
