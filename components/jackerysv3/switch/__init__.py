import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_JACKERYSV3_ID, MAX_PLUGS, JackerySV3Hub, jackerysv3_ns

DEPENDENCIES = ["jackerysv3"]

JackerySV3Switch = jackerysv3_ns.class_("JackerySV3Switch", switch.Switch, cg.Component, cg.Parented.template(JackerySV3Hub))

CONF_PLUGS = "plugs"
CONF_AC_SOCKET = "ac_socket"
CONF_AUTO_STANDBY_ALLOWED = "auto_standby_allowed"

# (config key, decoder key, schema attributes). Decoder keys: see SWITCH_KINDS in jackery_state.cpp.
SWITCHES = [
    (CONF_AC_SOCKET, "ac_socket", dict(icon="mdi:power-socket-eu")),
    (CONF_AUTO_STANDBY_ALLOWED, "auto_standby_allowed", dict(icon="mdi:power-sleep", entity_category=ENTITY_CATEGORY_CONFIG)),
]

# Smart plugs (0-indexed slots: plug0 .. plug9). Only plugs connected locally (commMode 1) accept commands.
PLUG_SCHEMA = switch.switch_schema(JackerySV3Switch, icon="mdi:power-socket-eu")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_JACKERYSV3_ID): cv.use_id(JackerySV3Hub),
        **{cv.Optional(key): switch.switch_schema(JackerySV3Switch, **attrs) for key, _flat, attrs in SWITCHES},
        cv.Optional(CONF_PLUGS): cv.Schema({cv.Optional(f"plug{i}"): PLUG_SCHEMA for i in range(MAX_PLUGS)}),
    }
)


async def _make(hub, conf, kind, index):
    var = await switch.new_switch(conf)
    await cg.register_component(var, conf)
    await cg.register_parented(var, hub)
    cg.add(var.set_kind(getattr(JackerySV3SwitchKind, kind)))
    cg.add(var.set_index(index))
    cg.add(hub.add_switch(getattr(JackerySV3SwitchKind, kind), index, var))


JackerySV3SwitchKind = jackerysv3_ns.enum("SwitchKind", True)
KIND_NAMES = {"ac_socket": "AC_SOCKET", "auto_standby_allowed": "AUTO_STANDBY_ALLOWED", "plug": "PLUG"}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JACKERYSV3_ID])
    for key, flat, _attrs in SWITCHES:
        if key in config:
            await _make(hub, config[key], KIND_NAMES[flat], 0)
    plugs = config.get(CONF_PLUGS) or {}
    for i in range(MAX_PLUGS):
        if f"plug{i}" in plugs:
            await _make(hub, plugs[f"plug{i}"], KIND_NAMES["plug"], i)
