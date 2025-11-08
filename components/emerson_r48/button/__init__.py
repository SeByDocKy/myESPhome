import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_CONFIG, CONF_ID

from .. import EmersonR48Component, emerson_r48_ns, CONF_EMERSON_R48_ID

EmersonR48Button = emerson_r48_ns.class_(
    "EmersonR48Button", button.Button, cg.Component
)

CONF_SET_OFFLINE_VALUES = "set_offline_values"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_EMERSON_R48_ID): cv.use_id(EmersonR48Component),
            cv.Optional(CONF_SET_OFFLINE_VALUES): button.BUTTON_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(EmersonR48Button),
                    cv.Optional(
                        CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
                    ): cv.entity_category,
                    cv.Required('name'): cv.string_strict
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EMERSON_R48_ID])
    if config[CONF_SET_OFFLINE_VALUES]:
        conf = config[CONF_SET_OFFLINE_VALUES]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await button.register_button(
            var,
            conf,
        )
        cg.add(var.set_parent(hub))
