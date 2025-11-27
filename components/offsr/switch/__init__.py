import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)

DEPENDENCIES = ["offsr"]

from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

ActivationSwitch = offsr_ns.class_("ActivationSwitch", switch.Switch, cg.Component)
ManualOverrideSwitch = offsr_ns.class_("ManualOverrideSwitch", switch.Switch, cg.Component)
PidModeSwitch = offsr_ns.class_("PidModeSwitch", switch.Switch, cg.Component)
ReverseSwitch = offsr_ns.class_("ReverseSwitch", switch.Switch, cg.Component)

CONF_ACTIVATION = "activation"
CONF_MANUAL_OVERRIDE = "manual_override"
CONF_PID_MODE = "pid_mode"
CONF_REVERSE = "reverse"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
    
    cv.Optional(CONF_ACTIVATION): switch.switch_schema(
        ActivationSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_MANUAL_OVERRIDE): switch.switch_schema(
        ManualOverrideSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,    
    ).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_PID_MODE): switch.switch_schema(
        PidModeSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,    
    ).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_REVERSE): switch.switch_schema(
        ReverseSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,    
    ).extend(cv.COMPONENT_SCHEMA),
}

async def to_code(config):
    offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    
    if activation_config := config.get(CONF_ACTIVATION):
        s = await switch.new_switch(activation_config)
        await cg.register_component(s, activation_config)
        await cg.register_parented(s, offsr_component)
        cg.add(offsr_component.set_activation_switch(s))
        
    if manual_override_config := config.get(CONF_MANUAL_OVERRIDE):
        s = await switch.new_switch(manual_override_config)
        await cg.register_component(s, manual_override_config)
        await cg.register_parented(s, offsr_component)
        cg.add(offsr_component.set_manual_override_switch(s))
        
    if pid_mode_config := config.get(CONF_PID_MODE):
        s = await switch.new_switch(pid_mode_config)
        await cg.register_component(s, pid_mode_config)
        await cg.register_parented(s, offsr_component)
        cg.add(offsr_component.set_pid_mode_switch(s)) 
        
    if reverse_config := config.get(CONF_REVERSE):
        s = await switch.new_switch(reverse_config)
        await cg.register_component(s, reverse_config)
        await cg.register_parented(s, offsr_component)
        cg.add(offsr_component.set_reverse_switch(s)) 
  


