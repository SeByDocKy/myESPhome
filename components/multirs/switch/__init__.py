import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_MULTIRS_ID, MultiRS, multirs_ns

DEPENDENCIES = ["multirs"]

MultiRSSwitch = multirs_ns.class_("MultiRSSwitch", switch.Switch, cg.Parented.template(MultiRS))

# key -> (SwitchKind value, schema). The numeric kinds mirror the C++ enum SwitchKind in multirs.h.
# All three live in register 0xD067 (AC input 1 control behaviour).
SWITCHES = {
    "ups_function": (0, dict(entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:battery-sync")),
    "generator_load_moderation": (1, dict(entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:engine")),
    "weak_ac_input": (2, dict(entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:transmission-tower-off")),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MULTIRS_ID): cv.use_id(MultiRS),
        **{
            cv.Optional(key): switch.switch_schema(MultiRSSwitch, **kwargs)
            for key, (_, kwargs) in SWITCHES.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MULTIRS_ID])
    for key, (kind, _) in SWITCHES.items():
        if key in config:
            var = await switch.new_switch(config[key])
            await cg.register_parented(var, config[CONF_MULTIRS_ID])
            cg.add(var.set_kind(kind))
            cg.add(hub.register_switch(var))
