import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_RESTART, ENTITY_CATEGORY_CONFIG

from .. import CONF_JACKERYSV3_ID, JackerySV3Hub, jackerysv3_ns

DEPENDENCIES = ["jackerysv3"]

JackerySV3RebootButton = jackerysv3_ns.class_(
    "JackerySV3RebootButton", button.Button, cg.Component, cg.Parented.template(JackerySV3Hub)
)

CONF_REBOOT = "reboot"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_JACKERYSV3_ID): cv.use_id(JackerySV3Hub),
        cv.Optional(CONF_REBOOT): button.button_schema(
            JackerySV3RebootButton,
            device_class=DEVICE_CLASS_RESTART,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:restart",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JACKERYSV3_ID])
    if CONF_REBOOT in config:
        var = await button.new_button(config[CONF_REBOOT])
        await cg.register_component(var, config[CONF_REBOOT])
        await cg.register_parented(var, hub)
