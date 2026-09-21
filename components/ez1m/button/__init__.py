import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import ENTITY_CATEGORY_CONFIG
from .. import ez1m_ns, EZ1MComponent, CONF_EZ1M_ID

DEPENDENCIES = ["ez1m"]

EZ1MButton = ez1m_ns.class_("EZ1MButton", button.Button, cg.Parented.template(EZ1MComponent))
EZ1MButtonType = ez1m_ns.enum("EZ1MButtonType", is_class=True)

CONF_SET_POWER_MIN = "set_power_min"
CONF_SET_POWER_MAX = "set_power_max"

# Both buttons write the hub's persisted "startup power limit" (applied at
# every boot, see ez1m.cpp::setup()) and apply it immediately as well.
BUTTON_TYPES = {
    CONF_SET_POWER_MIN: button.button_schema(
        EZ1MButton,
        icon="mdi:arrow-collapse-down",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    CONF_SET_POWER_MAX: button.button_schema(
        EZ1MButton,
        icon="mdi:arrow-collapse-up",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EZ1M_ID): cv.use_id(EZ1MComponent),
        **{cv.Optional(key): schema for key, schema in BUTTON_TYPES.items()},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EZ1M_ID])
    for key in BUTTON_TYPES:
        if key not in config:
            continue
        conf = config[key]
        var = await button.new_button(conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_kind(getattr(EZ1MButtonType, key.upper())))
