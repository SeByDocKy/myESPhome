import esphome.codegen as cg
from esphome.components import switch
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

BluettiSwitch = bluetti_ns.class_("BluettiSwitch", switch.Switch, cg.Component)

# ac_output (Balco 260) / system_power (Balco 500) / grid_charging (Balco 260) / grid_feed_in
SPECS = union_specs("switch")
TREE = build_tree(SPECS)


def _schema(key):
    # restore_mode DISABLED: nothing is written to the device at boot (system_power switches the whole system off)
    return switch.switch_schema(
        BluettiSwitch,
        default_restore_mode="DISABLED",
        **entity_kwargs(SPECS[key], "icon", "entity_category"),
    )


CONFIG_SCHEMA = BLUETTI_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("switch", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BLUETTI_ID])
    hub_conf = hub_config(config[CONF_BLUETTI_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["switch"]

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        spec = available[key]
        var = await switch.new_switch(conf)
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
