import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import CONF_PCM3K6W_ID, PCM3K6W_PLATFORM_SCHEMA, pcm3k6w_ns

DEPENDENCIES = ["canbus"]

PCM3K6WSwitch = pcm3k6w_ns.class_("PCM3K6WSwitch", switch.Switch, cg.Component)

# Kind indices MUST match the SwitchKind enum order in pcm3k6w.h.
KIND_POWER, KIND_DISCHARGE_CHARGE, KIND_MANUAL_FAN_CONTROL = range(3)

# (config key, kind, icon)
SWITCHES = [
    ("power", KIND_POWER, "mdi:power"),
    ("discharge_charge", KIND_DISCHARGE_CHARGE, "mdi:transmission-tower"),
    ("manual_fan_control", KIND_MANUAL_FAN_CONTROL, "mdi:fan"),
]

CONFIG_SCHEMA = PCM3K6W_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): switch.switch_schema(PCM3K6WSwitch, icon=icon).extend(cv.COMPONENT_SCHEMA)
        for key, _kind, icon in SWITCHES
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PCM3K6W_ID])
    for key, kind, _icon in SWITCHES:
        if key in config:
            s = await switch.new_switch(config[key])
            await cg.register_component(s, config[key])
            await cg.register_parented(s, hub)
            cg.add(s.set_kind(kind))
            cg.add(hub.set_switch(kind, s))
