import esphome.codegen as cg
from esphome.components import text_sensor
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

BluettiTextSensor = bluetti_ns.class_("BluettiTextSensor", text_sensor.TextSensor, cg.Component)
TextKind = bluetti_ns.enum("TextKind", True)

KINDS = {
    "string": TextKind.STRING,
    "serial": TextKind.SERIAL,
    "version": TextKind.VERSION,
    "states": TextKind.STATES,
    "flags": TextKind.FLAGS,
    "hex": TextKind.HEX,
}

SPECS = union_specs("text_sensor")
TREE = build_tree(SPECS)


def _schema(key):
    return text_sensor.text_sensor_schema(
        BluettiTextSensor, **entity_kwargs(SPECS[key], "icon", "entity_category")
    )


CONFIG_SCHEMA = BLUETTI_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("text_sensor", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BLUETTI_ID])
    hub_conf = hub_config(config[CONF_BLUETTI_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["text_sensor"]

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        spec = available[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)

        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                MODBUS_REGISTER_TYPE["holding"],
            )
        )
        cg.add(var.set_kind(KINDS[spec["kind"]]))
        for value, label in spec.get("states", {}).items():
            cg.add(var.add_state(value, label))
        for bit, label in spec.get("flags", {}).items():
            cg.add(var.add_flag(bit, label))
        cg.add(hub.add_item(var, is_slow(spec)))
