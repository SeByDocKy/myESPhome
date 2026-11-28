import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

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

EcoflowStreamSwitch = ecoflow_stream_ns.class_("EcoflowStreamSwitch", switch.Switch, cg.Component)
EcoflowStreamHeartbeatSwitch = ecoflow_stream_ns.class_("EcoflowStreamHeartbeatSwitch", switch.Switch, cg.Component)

# Not a register: stops / restarts the heartbeat of the hub (off = the battery manages itself again)
CONF_HEARTBEAT = "heartbeat"

SPECS = union_specs("switch")
TREE = build_tree(SPECS)


def _schema(key):
    # restore_mode DISABLED: nothing is written to the device at boot
    return switch.switch_schema(
        EcoflowStreamSwitch, default_restore_mode="DISABLED", **entity_kwargs(SPECS[key], "icon", "entity_category")
    )


CONFIG_SCHEMA = ECOFLOW_STREAM_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema)).extend(
    {
        cv.Optional(CONF_HEARTBEAT): switch.switch_schema(
            EcoflowStreamHeartbeatSwitch,
            default_restore_mode="RESTORE_DEFAULT_ON",
            icon="mdi:heart-pulse",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("switch", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ECOFLOW_STREAM_ID])
    hub_conf = hub_config(config[CONF_ECOFLOW_STREAM_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["switch"]

    if CONF_HEARTBEAT in config:
        var = await switch.new_switch(config[CONF_HEARTBEAT])
        await cg.register_component(var, config[CONF_HEARTBEAT])
        await cg.register_parented(var, hub)
        cg.add(hub.enable_control())

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(hub.enable_control())
        configure_item(var, hub, available[key])
