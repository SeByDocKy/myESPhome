import esphome.codegen as cg
from esphome.components import select

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

EcoflowStreamSelect = ecoflow_stream_ns.class_("EcoflowStreamSelect", select.Select, cg.Component)

SPECS = union_specs("select")
TREE = build_tree(SPECS)


def _schema(key):
    return select.select_schema(EcoflowStreamSelect, **entity_kwargs(SPECS[key], "icon", "entity_category"))


CONFIG_SCHEMA = ECOFLOW_STREAM_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("select", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ECOFLOW_STREAM_ID])
    hub_conf = hub_config(config[CONF_ECOFLOW_STREAM_ID])
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
        for _, value in options:
            cg.add(var.add_value(value))
        cg.add(hub.enable_control())
        configure_item(var, hub, spec)
