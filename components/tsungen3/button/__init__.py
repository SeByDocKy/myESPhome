import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_TSUNGEN3_ID, TSunGen3Component, tsungen3_ns

DEPENDENCIES = ["tsungen3"]

CONF_RESET_TSUNGEN3 = "reset_tsungen3"

TSunGen3ResetButton = tsungen3_ns.class_(
    "TSunGen3ResetButton", button.Button, cg.Parented.template(TSunGen3Component)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSUNGEN3_ID): cv.use_id(TSunGen3Component),
        cv.Optional(CONF_RESET_TSUNGEN3): button.button_schema(
            TSunGen3ResetButton,
            icon="mdi:restart",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_TSUNGEN3_ID])
    if CONF_RESET_TSUNGEN3 in config:
        conf = config[CONF_RESET_TSUNGEN3]
        var = await button.new_button(conf)
        await cg.register_parented(var, hub)
