import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_RYLR999_BLE_ID, rylr999_ble_ns, RYLR999BLEComponent

DEPENDENCIES = ["rylr999_ble"]

CONF_RESET = "reset"

RYLR999BLEResetButton = rylr999_ble_ns.class_(
    "RYLR999BLEResetButton", button.Button, cg.Parented.template(RYLR999BLEComponent)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RYLR999_BLE_ID): cv.use_id(RYLR999BLEComponent),
        cv.Optional(CONF_RESET): button.button_schema(
            RYLR999BLEResetButton,
            device_class=DEVICE_CLASS_RESTART,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:restart",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RYLR999_BLE_ID])

    if CONF_RESET in config:
        b = await button.new_button(config[CONF_RESET])
        await cg.register_parented(b, parent)
