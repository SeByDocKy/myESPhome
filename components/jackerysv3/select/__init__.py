import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_JACKERYSV3_ID, JackerySV3Hub, jackerysv3_ns

DEPENDENCIES = ["jackerysv3"]

JackerySV3Select = jackerysv3_ns.class_("JackerySV3Select", select.Select, cg.Component, cg.Parented.template(JackerySV3Hub))
SelectKind = jackerysv3_ns.enum("SelectKind", True)

# (config key, SelectKind member, options, icon). The index of an option is the value written to the battery.
SELECTS = [
    # autoStandby: 0 = invalid, 1 = standby, 2 = on
    ("auto_standby_mode", "AUTO_STANDBY_MODE", ["Invalid", "Standby", "On"], "mdi:power-sleep"),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_JACKERYSV3_ID): cv.use_id(JackerySV3Hub),
        **{
            cv.Optional(key): select.select_schema(
                JackerySV3Select, icon=icon, entity_category=ENTITY_CATEGORY_CONFIG
            )
            for key, _kind, _options, icon in SELECTS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JACKERYSV3_ID])
    for key, kind, options, _icon in SELECTS:
        if key in config:
            var = await select.new_select(config[key], options=options)
            await cg.register_component(var, config[key])
            await cg.register_parented(var, hub)
            cg.add(var.set_kind(getattr(SelectKind, kind)))
            cg.add(hub.add_select(getattr(SelectKind, kind), var))
