import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ENTITY_CATEGORY_DIAGNOSTIC

from .. import APSInverter, CONF_INVERTER, apszb_ns

DEPENDENCIES = ["apszb"]

APSButton = apszb_ns.class_("APSButton", button.Button)

CONF_PAIR = "pair"
CONF_REFRESH = "refresh"
CONF_QUERY_MAX_POWER = "query_max_power"
CONF_REBOOT_INVERTER = "reboot_inverter"

# key -> (action code, schema)
BUTTONS = {
    CONF_PAIR: (
        0,
        button.button_schema(APSButton, entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:link-variant"),
    ),
    CONF_REFRESH: (
        1,
        button.button_schema(APSButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:refresh"),
    ),
    CONF_QUERY_MAX_POWER: (
        2,
        button.button_schema(APSButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:speedometer"),
    ),
    CONF_REBOOT_INVERTER: (
        3,
        button.button_schema(APSButton, entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:restart"),
    ),
}

_schema = {cv.Required(CONF_INVERTER): cv.use_id(APSInverter)}
for _key, (_code, _sch) in BUTTONS.items():
    _schema[cv.Optional(_key)] = _sch

CONFIG_SCHEMA = cv.Schema(_schema)


async def to_code(config):
    inv = await cg.get_variable(config[CONF_INVERTER])
    for key, (code, _) in BUTTONS.items():
        if key in config:
            btn = await button.new_button(config[key])
            await cg.register_parented(btn, inv)
            cg.add(btn.set_action(code))
