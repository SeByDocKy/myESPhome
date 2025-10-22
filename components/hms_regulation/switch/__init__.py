import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)

DEPENDENCIES = ["hms_regulation"]

from .. import CONF_HMS_REGULATION_ID, HMS_REGULATIONComponent, hms_regulation_ns


ActivationSwitch = hms_regulation_ns.class_("ActivationSwitch", switch.Switch, cg.Component)
ManualOverrideSwitch = hms_regulation_ns.class_("ManualOverrideSwitch", switch.Switch, cg.Component)
PidModeSwitch = hms_regulation_ns.class_("PidModeSwitch", switch.Switch, cg.Component)

CONF_ACTIVATION = "activation"
CONF_MANUAL_OVERRIDE = "manual_override"
CONF_PID_MODE = "pid_mode"

CONFIG_SCHEMA = {

    cv.GenerateID(CONF_HMS_REGULATION_ID): cv.use_id(HMS_REGULATIONComponent),
    
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
}

async def to_code(config):
    hms_regulation_component = await cg.get_variable(config[CONF_HMS_REGULATION_ID])
    
    if activation_config := config.get(CONF_ACTIVATION):
        s = await switch.new_switch(activation_config)
        await cg.register_component(s, activation_config)
        await cg.register_parented(s, hms_regulation_component)
        cg.add(hms_regulation_component.set_activation_switch(s))
        
    if manual_override_config := config.get(CONF_MANUAL_OVERRIDE):
        s = await switch.new_switch(manual_override_config)
        await cg.register_component(s, manual_override_config)
        await cg.register_parented(s, hms_regulation_component)
        cg.add(hms_regulation_component.set_manual_override_switch(s))
        
    if pid_mode_config := config.get(CONF_PID_MODE):
        s = await switch.new_switch(pid_mode_config)
        await cg.register_component(s, pid_mode_config)
        await cg.register_parented(s, hms_regulation_component)
        cg.add(hms_regulation_component.set_pid_mode_switch(s))        
