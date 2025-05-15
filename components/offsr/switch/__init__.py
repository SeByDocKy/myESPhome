import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)

DEPENDENCIES = ["offsr"]

from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

# OFFSRSwitch = offsr_ns.class_("OFFSRSwitch", switch.Switch)

ActivationSwitch = offsr_ns.class_("ActivationSwitch", switch.Switch, cg.Component)
ManualOverrideSwitch = offsr_ns.class_("ManualOverrideSwitch", switch.Switch, cg.Component)
PidModeSwitch = offsr_ns.class_("PidModeSwitch", switch.Switch, cg.Component)

CONF_ACTIVATION = "activation"
CONF_MANUAL_OVERRIDE = "manual_override"
CONF_PID_MODE = "pid_mode"

CONFIG_SCHEMA = {
#     cv.GenerateID(): cv.declare_id(OFFSRSwitch),

    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
    
    cv.Optional(CONF_ACTIVATION): switch.switch_schema(
        ActivationSwitch,
#        OFFSRSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_MANUAL_OVERRIDE): switch.switch_schema(
        ManualOverrideSwitch,
#        OFFSRSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,    
    ).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_PID_MODE): switch.switch_schema(
        PidModeSwitch,
#        OFFSRSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,    
    ).extend(cv.COMPONENT_SCHEMA),
}

async def to_code(config):
    # var = cg.new_Pvariable(config[CONF_ID])
    # await cg.register_component(var, config)
    # offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    # cg.add(var.set_parent(offsr_component))
  
    # var = await cg.get_variable(config[CONF_OFFSR_ID])
    # if CONF_ACTIVATION in config:
        # s = await switch.new_switch(config[CONF_ACTIVATION])
        # await cg.register_parented(s, config[CONF_OFFSR_ID])
        # cg.add(var.set_activation_switch(s))
        
    # if CONF_MANUAL_OVERRIDE in config:
        # s = await switch.new_switch(config[CONF_MANUAL_OVERRIDE])
        # await cg.register_parented(s, config[CONF_OFFSR_ID])
        # cg.add(var.set_manual_override_switch(s))
        

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
       
    