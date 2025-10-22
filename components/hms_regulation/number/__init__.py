import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_POWER,
    ENTITY_CATEGORY_CONFIG,
    ICON_POWER,
    UNIT_WATT,
)

DEPENDENCIES = ["hms_regulation"]


from .. import CONF_HMS_REGULATION_ID, HMS_REGULATIONComponent, hms_regulation_ns

SetpointNumber = hms_regulation_ns.class_("SetpointNumber", number.Number, cg.Component)

KpNumber = hms_regulation_ns.class_("KpNumber", number.Number, cg.Component)
KiNumber = hms_regulation_ns.class_("KiNumber", number.Number, cg.Component)
KdNumber = hms_regulation_ns.class_("KdNumber", number.Number, cg.Component)

OutputMinNumber = hms_regulation_ns.class_("OutputMinNumber", number.Number, cg.Component)
OutputMaxNumber = hms_regulation_ns.class_("OutputMaxNumber", number.Number, cg.Component)

CONF_SETPOINT = "setpoint"

CONF_KP = "kp"
CONF_KI = "ki"
CONF_KD = "kd"

CONF_OUTPUT_MIN = "output_min"
CONF_OUTPUT_MAX = "output_max"

CONFIG_SCHEMA = {
     
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(HMS_REGULATIONComponent),
    
    cv.Optional(CONF_SETPOINT): number.number_schema(
        ChargingSetpointNumber,
        device_class=DEVICE_CLASS_POWER,
        icon = ICON_POWER,
        unit_of_measurement=UNIT_WATT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
        
    cv.Optional(CONF_KP): number.number_schema(
        KpNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_KI): number.number_schema(
        KiNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_KD): number.number_schema(
        KdNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_OUTPUT_MIN): number.number_schema(
        OutputMinNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_OUTPUT_MAX): number.number_schema(
        OutputMaxNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
}

async def to_code(config):
  hms_regulation_component = await cg.get_variable(config[CONF_HMS_REGULATION_ID])
  if setpoint_config := config.get(CONF_SETPOINT):
        n = await number.new_number(
            setpoint_config, min_value=-200.0, max_value=200.0, step=1
        )
        await cg.register_component(n, setpoint_config)
        await cg.register_parented(n, hms_regulation_component)
        cg.add(hms_regulation_component.set_charging_setpoint_number(n))
        
  if kp_config := config.get(CONF_KP):
        n = await number.new_number(
            kp_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kp_config)
        await cg.register_parented(n, hms_regulation_component)
        cg.add(hms_regulation_component.set_kp_number(n)) 
  
  if ki_config := config.get(CONF_KI):
        n = await number.new_number(
            ki_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, ki_config)
        await cg.register_parented(n, hms_regulation_component)
        cg.add(hms_regulation_component.set_ki_number(n))

  if kd_config := config.get(CONF_KD):
        n = await number.new_number(
            kd_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kd_config)
        await cg.register_parented(n, hms_regulation_component)
        cg.add(hms_regulation_component.set_kd_number(n))


  if output_min_config := config.get(CONF_OUTPUT_MIN):
        n = await number.new_number(
            output_min_config, min_value=0.0, max_value=1.0, step=0.01
        )
        await cg.register_component(n, output_min_config)
        await cg.register_parented(n, hms_regulation_component)
        cg.add(hms_regulation_component.set_output_min_number(n))

  if output_max_config := config.get(CONF_OUTPUT_MAX):
        n = await number.new_number(
            output_max_config, min_value=0.0, max_value=1.0, step=0.01
        )
        await cg.register_component(n, output_max_config)
        await cg.register_parented(n, hms_regulation_component)
        cg.add(hms_regulation_component.set_output_max_number(n))
