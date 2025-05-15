import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_CONFIG,
    ICON_BATTERY,
    UNIT_AMPERE,
    UNIT_VOLT,
)

DEPENDENCIES = ["offsr"]

ICON_CURRENT_DC = "mdi:current-dc"
ICON_SINE_WAVE = "mdi:sine-wave"

from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

ChargingSetpointNumber = offsr_ns.class_("ChargingSetpointNumber", number.Number, cg.Component)
AbsorbingSetpointNumber = offsr_ns.class_("AbsorbingSetpointNumber", number.Number, cg.Component)
FloatingSetpointNumber = offsr_ns.class_("FloatingSetpointNumber", number.Number, cg.Component)
StartingBatteryVoltageNumber = offsr_ns.class_("StartingBatteryVoltageNumber", number.Number, cg.Component)
ChargedBatteryVoltageNumber = offsr_ns.class_("ChargedBatteryVoltageNumber", number.Number, cg.Component)
DischargedBatteryVoltageNumber = offsr_ns.class_("DischargedBatteryVoltageNumber", number.Number, cg.Component)
KpNumber = offsr_ns.class_("KpNumber", number.Number, cg.Component)
KiNumber = offsr_ns.class_("KiNumber", number.Number, cg.Component)
KdNumber = offsr_ns.class_("KdNumber", number.Number, cg.Component)
OutputMinNumber = offsr_ns.class_("OutputMinNumber", number.Number, cg.Component)
OutputMaxNumber = offsr_ns.class_("OutputMaxNumber", number.Number, cg.Component)
OutputRestartNumber = offsr_ns.class_("OutputRestartNumber", number.Number, cg.Component)

CONF_CHARGING_SETPOINT = "charging_setpoint"
CONF_ABSORBING_SETPOINT = "absorbing_setpoint"
CONF_FLOATING_SETPOINT = "floating_setpoint"

CONF_STARTING_BATTERY_VOLTAGE = "starting_battery_voltage"
CONF_CHARGED_BATTERY_VOLTAGE = "charged_battery_voltage"
CONF_DISCHARGED_BATTERY_VOLTAGE = "discharged_battery_voltage"

CONF_KP = "kp"
CONF_KI = "ki"
CONF_KD = "kd"

CONF_OUTPUT_MIN = "output_min"
CONF_OUTPUT_MAX = "output_max"
CONF_OUTPUT_RESTART = "output_restart"

CONFIG_SCHEMA = {
     
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
    
    cv.Optional(CONF_CHARGING_SETPOINT): number.number_schema(
        ChargingSetpointNumber,
        device_class=DEVICE_CLASS_CURRENT,
        icon = ICON_CURRENT_DC,
        unit_of_measurement=UNIT_AMPERE,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_ABSORBING_SETPOINT): number.number_schema(
        AbsorbingSetpointNumber,
        device_class=DEVICE_CLASS_CURRENT,
        icon = ICON_CURRENT_DC,
        unit_of_measurement=UNIT_AMPERE,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_FLOATING_SETPOINT): number.number_schema(
        FloatingSetpointNumber,
        device_class=DEVICE_CLASS_CURRENT,
        icon = ICON_CURRENT_DC,
        unit_of_measurement=UNIT_AMPERE,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_STARTING_BATTERY_VOLTAGE): number.number_schema(
        StartingBatteryVoltageNumber,
        device_class=DEVICE_CLASS_VOLTAGE,
        icon = ICON_SINE_WAVE,
        unit_of_measurement=UNIT_VOLT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_CHARGED_BATTERY_VOLTAGE): number.number_schema(
        ChargedBatteryVoltageNumber,
        device_class=DEVICE_CLASS_VOLTAGE,
        unit_of_measurement=UNIT_VOLT,
        icon = ICON_SINE_WAVE,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_DISCHARGED_BATTERY_VOLTAGE): number.number_schema(
        DischargedBatteryVoltageNumber,
        device_class=DEVICE_CLASS_VOLTAGE,
        icon = ICON_SINE_WAVE,
        unit_of_measurement=UNIT_VOLT,
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
    
    cv.Optional(CONF_OUTPUT_RESTART): number.number_schema(
        OutputRestartNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),                
}

async def to_code(config):
  offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
  if charging_setpoint_config := config.get(CONF_CHARGING_SETPOINT):
        n = await number.new_number(
            charging_setpoint_config, min_value=0.0, max_value=50.0, step=0.2
        )
        await cg.register_component(n, charging_setpoint_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_charging_setpoint_number(n))
        
  if absorbing_setpoint_config := config.get(CONF_ABSORBING_SETPOINT):
        n = await number.new_number(
            absorbing_setpoint_config, min_value=0.0, max_value=20.0, step=0.2
        )
        await cg.register_component(n, absorbing_setpoint_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_absorbing_setpoint_number(n))

  if floating_setpoint_config := config.get(CONF_FLOATING_SETPOINT):
        n = await number.new_number(
            floating_setpoint_config, min_value=-2.0, max_value=2.0, step=0.2
        )
        await cg.register_component(n, floating_setpoint_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_floating_setpoint_number(n))     
        
  if starting_battery_voltage_config := config.get(CONF_STARTING_BATTERY_VOLTAGE):
        n = await number.new_number(
            starting_battery_voltage_config, min_value=50.0, max_value=60.0, step=0.1
        )
        await cg.register_component(n, starting_battery_voltage_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_starting_battery_voltage_number(n))

  if charged_battery_voltage_config := config.get(CONF_CHARGED_BATTERY_VOLTAGE):
        n = await number.new_number(
            charged_battery_voltage_config, min_value=50.0, max_value=60.0, step=0.1
        )
        await cg.register_component(n, charged_battery_voltage_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_charged_battery_voltage_number(n))

  if discharged_battery_voltage_config := config.get(CONF_DISCHARGED_BATTERY_VOLTAGE):
        n = await number.new_number(
            discharged_battery_voltage_config, min_value=50.0, max_value=60.0, step=0.1
        )
        await cg.register_component(n, discharged_battery_voltage_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_discharged_battery_voltage_number(n))  


  if kp_config := config.get(CONF_KP):
        n = await number.new_number(
            kp_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kp_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_kp_number(n)) 
  
  if ki_config := config.get(CONF_KI):
        n = await number.new_number(
            ki_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, ki_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_ki_number(n))

  if kd_config := config.get(CONF_KD):
        n = await number.new_number(
            kd_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kd_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_kd_number(n))


  if output_min_config := config.get(CONF_OUTPUT_MIN):
        n = await number.new_number(
            output_min_config, min_value=0.0, max_value=1.0, step=0.01
        )
        await cg.register_component(n, output_min_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_output_min_number(n))

  if output_max_config := config.get(CONF_OUTPUT_MAX):
        n = await number.new_number(
            output_max_config, min_value=0.0, max_value=1.0, step=0.01
        )
        await cg.register_component(n, output_max_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_output_max_number(n))

  if output_restart_config := config.get(CONF_OUTPUT_RESTART):
        n = await number.new_number(
            output_restart_config, min_value=0.0, max_value=1.0, step=0.01
        )
        await cg.register_component(n, output_restart_config)
        await cg.register_parented(n, offsr_component)
        cg.add(offsr_component.set_output_restart_number(n))        
