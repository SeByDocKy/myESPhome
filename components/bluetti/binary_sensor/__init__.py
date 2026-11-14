import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

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

BluettiBinarySensor = bluetti_ns.class_("BluettiBinarySensor", binary_sensor.BinarySensor, cg.Component)

# The connection sensor is a plain BinarySensor driven by the hub: no register behind it
CONF_MODBUS_CONNECTION = "modbus_connection"

SPECS = union_specs("binary_sensor")
TREE = build_tree(SPECS)


def _schema(key):
    return binary_sensor.binary_sensor_schema(
        BluettiBinarySensor, **entity_kwargs(SPECS[key], "icon", "entity_category", "device_class")
    )


CONFIG_SCHEMA = (
    BLUETTI_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
    .extend(
        {
            cv.Optional(CONF_MODBUS_CONNECTION): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:lan-connect",
            ),
        }
    )
)
_FINAL_VALIDATE = final_validate_platform("binary_sensor", TREE, SPECS)
FINAL_VALIDATE_SCHEMA = _FINAL_VALIDATE


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BLUETTI_ID])
    hub_conf = hub_config(config[CONF_BLUETTI_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["binary_sensor"]

    if CONF_MODBUS_CONNECTION in config:
        var = await binary_sensor.new_binary_sensor(config[CONF_MODBUS_CONNECTION])
        cg.add(hub.set_connection_sensor(var))

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        spec = available[key]
        var = await binary_sensor.new_binary_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                MODBUS_REGISTER_TYPE["holding"],
            )
        )
        cg.add(hub.add_item(var, is_slow(spec)))
