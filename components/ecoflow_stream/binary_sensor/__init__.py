import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from .. import (
    ECOFLOW_STREAM_PLATFORM_SCHEMA,
    CONF_ECOFLOW_STREAM_ID,
    CONF_MODEL,
    build_tree,
    configure_item,
    ecoflow_stream_ns,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    tree_schema,
    union_specs,
    walk_config,
)
from ..registers import REGISTERS

DEPENDENCIES = ["ecoflow_stream"]

EcoflowStreamBinarySensor = ecoflow_stream_ns.class_(
    "EcoflowStreamBinarySensor", binary_sensor.BinarySensor, cg.Component
)

# The connection sensor is a plain BinarySensor driven by the hub: no register behind it
CONF_MODBUS_CONNECTION = "modbus_connection"

SPECS = union_specs("binary_sensor")
TREE = build_tree(SPECS)


def _schema(key):
    return binary_sensor.binary_sensor_schema(
        EcoflowStreamBinarySensor, **entity_kwargs(SPECS[key], "icon", "entity_category", "device_class")
    )


CONFIG_SCHEMA = ECOFLOW_STREAM_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema)).extend(
    {
        cv.Optional(CONF_MODBUS_CONNECTION): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:lan-connect",
        ),
    }
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("binary_sensor", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ECOFLOW_STREAM_ID])
    hub_conf = hub_config(config[CONF_ECOFLOW_STREAM_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["binary_sensor"]

    if CONF_MODBUS_CONNECTION in config:
        var = await binary_sensor.new_binary_sensor(config[CONF_MODBUS_CONNECTION])
        cg.add(hub.set_connection_sensor(var))

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        var = await binary_sensor.new_binary_sensor(conf)
        await cg.register_component(var, conf)
        configure_item(var, hub, available[key])
