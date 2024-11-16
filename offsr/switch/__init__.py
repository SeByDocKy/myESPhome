import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

ActivationSwitch = offsr_ns.class_("ActivationSwitch", switch.Switch)

CONF_ACTIVATION = "activation"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
    cv.Optional(CONF_ACTIVATION): switch.switch_schema(
        ActivationSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}

async def to_code(config):
    offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    if activation_config := config.get(CONF_ACTIVATION):
        s = await switch.new_switch(activation_config)
        await cg.register_parented(s, config[CONF_OFFSR_ID])
        cg.add(offsr_component.set_activation_switch(s))
       
    