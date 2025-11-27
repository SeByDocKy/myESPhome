import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)

DEPENDENCIES = ["bioffsr"]

from .. import CONF_BIOFFSR_ID, BIOFFSRComponent, bioffsr_ns

ActivationSwitch = bioffsr_ns.class_("ActivationSwitch", switch.Switch, cg.Component)
ManualOverrideSwitch = bioffsr_ns.class_("ManualOverrideSwitch", switch.Switch, cg.Component)
PidModeSwitch = bioffsr_ns.class_("PidModeSwitch", switch.Switch, cg.Component)
ReverseSwitch = bioffsr_ns.class_("ReverseSwitch", switch.Switch, cg.Component)
Output1ActivationSwitch = bioffsr_ns.class_("Output1ActivationSwitch", switch.Switch, cg.Component)
Output2ActivationSwitch = bioffsr_ns.class_("Output2ActivationSwitch", switch.Switch, cg.Component)

CONF_ACTIVATION = "activation"
CONF_MANUAL_OVERRIDE = "manual_override"
CONF_PID_MODE = "pid_mode"
CONF_REVERSE = "reverse"
CONF_OUTPUT1_ACTIVATION = "output1_activation"
CONF_OUTPUT2_ACTIVATION = "output2_activation"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_BIOFFSR_ID): cv.use_id(BIOFFSRComponent),
    
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
    cv.Optional(CONF_OUTPUT1_ACTIVATION): switch.switch_schema(
        Output1ActivationSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,    
    ).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_OUTPUT2_ACTIVATION): switch.switch_schema(
        Output2ActivationSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,    
    ).extend(cv.COMPONENT_SCHEMA),
}

async def to_code(config):
    bioffsr_component = await cg.get_variable(config[CONF_BIOFFSR_ID])
    
    if activation_config := config.get(CONF_ACTIVATION):
        s = await switch.new_switch(activation_config)
        await cg.register_component(s, activation_config)
        await cg.register_parented(s, bioffsr_component)
        cg.add(bioffsr_component.set_activation_switch(s))
        
    if manual_override_config := config.get(CONF_MANUAL_OVERRIDE):
        s = await switch.new_switch(manual_override_config)
        await cg.register_component(s, manual_override_config)
        await cg.register_parented(s, bioffsr_component)
        cg.add(bioffsr_component.set_manual_override_switch(s))
        
    if pid_mode_config := config.get(CONF_PID_MODE):
        s = await switch.new_switch(pid_mode_config)
        await cg.register_component(s, pid_mode_config)
        await cg.register_parented(s, bioffsr_component)
        cg.add(bioffsr_component.set_pid_mode_switch(s)) 
        
    if output1_activation_config := config.get(CONF_OUTPUT1_ACTIVATION):
        s = await switch.new_switch(output1_activation_config)
        await cg.register_component(s, output1_activation_config)
        await cg.register_parented(s, bioffsr_component)
        cg.add(bioffsr_component.set_output1_activation_switch(s))
     
    if output2_activation_config := config.get(CONF_OUTPUT2_ACTIVATION):
        s = await switch.new_switch(output2_activation_config)
        await cg.register_component(s, output2_activation_config)
        await cg.register_parented(s, bioffsr_component)
        cg.add(bioffsr_component.set_output2_activation_switch(s))   
  


