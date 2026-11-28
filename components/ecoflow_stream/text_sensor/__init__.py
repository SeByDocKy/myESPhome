import esphome.codegen as cg
from esphome.components import text_sensor

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

EcoflowStreamTextSensor = ecoflow_stream_ns.class_("EcoflowStreamTextSensor", text_sensor.TextSensor, cg.Component)
TextKind = ecoflow_stream_ns.enum("TextKind", True)

KINDS = {
    "string": TextKind.STRING,
    "version": TextKind.VERSION,
    "decimal": TextKind.DECIMAL,
    "states": TextKind.STATES,
    "codes": TextKind.CODES,
}

SPECS = union_specs("text_sensor")
TREE = build_tree(SPECS)


def _schema(key):
    return text_sensor.text_sensor_schema(
        EcoflowStreamTextSensor, **entity_kwargs(SPECS[key], "icon", "entity_category")
    )


CONFIG_SCHEMA = ECOFLOW_STREAM_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("text_sensor", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ECOFLOW_STREAM_ID])
    hub_conf = hub_config(config[CONF_ECOFLOW_STREAM_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["text_sensor"]

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        spec = available[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_kind(KINDS[spec["kind"]]))
        for value, label in spec.get("states", {}).items():
            cg.add(var.add_state(value, label))
        configure_item(var, hub, spec)
