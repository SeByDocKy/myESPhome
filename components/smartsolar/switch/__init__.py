import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_SMARTSOLAR_ID, SmartSolar, smartsolar_ns

DEPENDENCIES = ["smartsolar"]

SmartSolarSwitch = smartsolar_ns.class_(
    "SmartSolarSwitch", switch.Switch, cg.Parented.template(SmartSolar)
)

# key -> (SwitchKind value, schema). The numeric kinds mirror the C++ enum SwitchKind in smartsolar.h.
SWITCHES = {
    "charger": (
        0,
        dict(entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:power"),
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SMARTSOLAR_ID): cv.use_id(SmartSolar),
        **{
            cv.Optional(key): switch.switch_schema(SmartSolarSwitch, **kwargs)
            for key, (_, kwargs) in SWITCHES.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SMARTSOLAR_ID])
    for key, (kind, _) in SWITCHES.items():
        if key in config:
            var = await switch.new_switch(config[key])
            await cg.register_parented(var, config[CONF_SMARTSOLAR_ID])
            cg.add(var.set_kind(kind))
            cg.add(hub.register_switch(var))
