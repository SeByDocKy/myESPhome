import esphome.codegen as cg
from esphome.components import select
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

BluettiSelect = bluetti_ns.class_("BluettiSelect", select.Select, cg.Component)

# working_mode: Custom / Self Consumption / Backup
SPECS = union_specs("select")
TREE = build_tree(SPECS)


def _schema(key):
    return select.select_schema(BluettiSelect, **entity_kwargs(SPECS[key], "icon", "entity_category"))


CONFIG_SCHEMA = BLUETTI_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("select", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BLUETTI_ID])
    hub_conf = hub_config(config[CONF_BLUETTI_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["select"]

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        spec = available[key]
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
                MODBUS_REGISTER_TYPE["holding"],
            )
        )
        for _, value in options:
            cg.add(var.add_value(value))
        cg.add(hub.add_item(var, is_slow(spec)))
