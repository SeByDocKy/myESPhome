import esphome.codegen as cg
from esphome.components import sensor

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

EcoflowStreamSensor = ecoflow_stream_ns.class_("EcoflowStreamSensor", sensor.Sensor, cg.Component)

SPECS = union_specs("sensor")
TREE = build_tree(SPECS)


def _schema(key):
    return sensor.sensor_schema(
        EcoflowStreamSensor,
        **entity_kwargs(
            SPECS[key],
            "unit_of_measurement",
            "icon",
            "entity_category",
            "device_class",
            "state_class",
            "accuracy_decimals",
        ),
    )


CONFIG_SCHEMA = ECOFLOW_STREAM_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("sensor", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ECOFLOW_STREAM_ID])
    hub_conf = hub_config(config[CONF_ECOFLOW_STREAM_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["sensor"]

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        configure_item(var, hub, available[key])
