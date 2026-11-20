import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_RYLR998_ID, rylr998_ns, RYLR998Component

DEPENDENCIES = ["rylr998"]

CONF_RESET = "reset"

RYLR998ResetButton = rylr998_ns.class_(
    "RYLR998ResetButton", button.Button, cg.Parented.template(RYLR998Component)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RYLR998_ID): cv.use_id(RYLR998Component),
        cv.Optional(CONF_RESET): button.button_schema(
            RYLR998ResetButton,
            device_class=DEVICE_CLASS_RESTART,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:restart",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RYLR998_ID])

    if CONF_RESET in config:
        b = await button.new_button(config[CONF_RESET])
        await cg.register_parented(b, parent)
