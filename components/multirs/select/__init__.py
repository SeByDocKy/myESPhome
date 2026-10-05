import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_MULTIRS_ID, MultiRS, multirs_ns

DEPENDENCIES = ["multirs"]

MultiRSSelect = multirs_ns.class_("MultiRSSelect", select.Select, cg.Parented.template(MultiRS))

# Option order = register value 1..5 of 0x0200 (see MODE_VALUES in multirs.cpp)
MODE_OPTIONS = ["Charger only", "Inverter only", "On", "Off", "Eco"]

# key -> (SelectKind value, schema, options). The numeric kinds mirror the C++ enum SelectKind in multirs.h.
SELECTS = {
    "mode": (0, dict(entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:power-settings"), MODE_OPTIONS),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MULTIRS_ID): cv.use_id(MultiRS),
        **{
            cv.Optional(key): select.select_schema(MultiRSSelect, **kwargs)
            for key, (_, kwargs, _) in SELECTS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MULTIRS_ID])
    for key, (kind, _, options) in SELECTS.items():
        if key in config:
            var = await select.new_select(config[key], options=options)
            await cg.register_parented(var, config[CONF_MULTIRS_ID])
            cg.add(var.set_kind(kind))
            cg.add(hub.register_select(var))
