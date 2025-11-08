import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

empty_switch_ns = cg.esphome_ns.namespace("empty_switch")
EmptySwitch = empty_switch_ns.class_("EmptySwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(EmptySwitch).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)