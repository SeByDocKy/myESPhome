import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MODE

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

EcoflowStreamNumber = ecoflow_stream_ns.class_("EcoflowStreamNumber", number.Number, cg.Component)

SPECS = union_specs("number")
TREE = build_tree(SPECS)


def _schema(key):
    spec = SPECS[key]
    # A slider cannot select a precise value over a wide range
    mode = "BOX" if spec["max"] - spec["min"] > 1000 else "SLIDER"
    return number.number_schema(
        EcoflowStreamNumber, **entity_kwargs(spec, "unit_of_measurement", "icon", "entity_category", "device_class")
    ).extend({cv.Optional(CONF_MODE, default=mode): cv.enum(number.NUMBER_MODES, upper=True)})


CONFIG_SCHEMA = ECOFLOW_STREAM_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("number", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ECOFLOW_STREAM_ID])
    hub_conf = hub_config(config[CONF_ECOFLOW_STREAM_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["number"]

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        spec = available[key]
        var = await number.new_number(conf, min_value=spec["min"], max_value=spec["max"], step=spec["step"])
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(hub.enable_control())
        configure_item(var, hub, spec)
