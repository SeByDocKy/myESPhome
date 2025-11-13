import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_POWER,
    ENTITY_CATEGORY_CONFIG,
    ICON_BATTERY,
    ICON_PERCENT,
    ICON_POWER,
    UNIT_VOLT,
    UNIT_PERCENT,
    UNIT_WATT,
)

DEPENDENCIES = ["dualpid"]

# ICON_CURRENT_DC = "mdi:current-dc"
ICON_SINE_WAVE = "mdi:sine-wave"

from .. import CONF_DUALPID_ID, DUALPIDComponent, dualpid_ns

SetpointNumber = dualpid_ns.class_("SetpointNumber", number.Number, cg.Component)

ChargingEpointNumber = dualpid_ns.class_("ChargingEpointNumber", number.Number, cg.Component)
AbsorbingEpointNumber = dualpid_ns.class_("AbsorbingEpointNumber", number.Number, cg.Component)
FloatingEpointNumber = dualpid_ns.class_("FloatingEpointNumber", number.Number, cg.Component)

StartingBatteryVoltageNumber = dualpid_ns.class_("StartingBatteryVoltageNumber", number.Number, cg.Component)
ChargedBatteryVoltageNumber = dualpid_ns.class_("ChargedBatteryVoltageNumber", number.Number, cg.Component)
DischargedBatteryVoltageNumber = dualpid_ns.class_("DischargedBatteryVoltageNumber", number.Number, cg.Component)

KpChargingNumber = dualpid_ns.class_("KpChargingNumber", number.Number, cg.Component)
KiChargingNumber = dualpid_ns.class_("KiChargingNumber", number.Number, cg.Component)
KdChargingNumber = dualpid_ns.class_("KdChargingNumber", number.Number, cg.Component)

KpDischargingNumber = dualpid_ns.class_("KpDischargingNumber", number.Number, cg.Component)
KiDischargingNumber = dualpid_ns.class_("KiDischargingNumber", number.Number, cg.Component)
KdDischargingNumber = dualpid_ns.class_("KdDischargingNumber", number.Number, cg.Component)

OutputMinNumber = dualpid_ns.class_("OutputMinNumber", number.Number, cg.Component)
OutputMaxNumber = dualpid_ns.class_("OutputMaxNumber", number.Number, cg.Component)


CONF_SETPOINT = "charging_epoint"

CONF_CHARGING_EPOINT = "charging_epoint"
CONF_ABSORBING_EPOINT = "absorbing_epoint"
CONF_FLOATING_EPOINT = "floating_epoint"

CONF_STARTING_BATTERY_VOLTAGE = "starting_battery_voltage"
CONF_CHARGED_BATTERY_VOLTAGE = "charged_battery_voltage"
CONF_DISCHARGED_BATTERY_VOLTAGE = "discharged_battery_voltage"

CONF_KP_CHARGING = "kp_charging"
CONF_KI_CHARGING = "ki_charging"
CONF_KD_CHARGING = "kd_charging"

CONF_KP_DISCHARGING = "kp_discharging"
CONF_KI_DISCHARGING = "ki_discharging"
CONF_KD_DISCHARGING = "kd_discharging"

CONF_OUTPUT_MIN = "output_min"
CONF_OUTPUT_MAX = "output_max"


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_DUALPID_ID): cv.use_id(DUALPIDComponent),
    
    cv.Optional(CONF_SETPOINT): number.number_schema(
        SetpointNumber,
        device_class=DEVICE_CLASS_POWER,
        icon = ICON_POWER,
        unit_of_measurement=UNIT_WATT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_CHARGING_EPOINT): number.number_schema(
        ChargingEpointNumber,
        device_class=DEVICE_CLASS_BATTERY,
        icon = ICON_PERCENT,
        unit_of_measurement=UNIT_PERCENT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_ABSORBING_EPOINT): number.number_schema(
        AbsorbingEpointNumber,
        device_class=DEVICE_CLASS_BATTERY,
        icon = ICON_PERCENT,
        unit_of_measurement=UNIT_PERCENT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_FLOATING_EPOINT): number.number_schema(
        FloatingEpointNumber,
        device_class=DEVICE_CLASS_BATTERY,
        icon = ICON_PERCENT,
        unit_of_measurement=UNIT_PERCENT,
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
    
    cv.Optional(CONF_KP_CHARGING): number.number_schema(
        KpChargingNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_KI_CHARGING): number.number_schema(
        KiChargingNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_KD_CHARGING): number.number_schema(
        KdChargingNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_KP_DISCHARGING): number.number_schema(
        KpDischargingNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_KI_DISCHARGING): number.number_schema(
        KiDischargingNumber,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_KD_DISCHARGING): number.number_schema(
        KdDischargingNumber,
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
  dualpid_component = await cg.get_variable(config[CONF_DUALPID_ID])
  
  if setpoint_config := config.get(CONF_SETPOINT):
        n = await number.new_number(
            setpoint_config, min_value=-400, max_value=400, step=10
        )
        await cg.register_component(n, setpoint_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_setpoint_number(n))
  
  if charging_epoint_config := config.get(CONF_CHARGING_EPOINT):
        n = await number.new_number(
            charging_epoint_config, min_value=0.0, max_value=0.5, step=0.05
        )
        await cg.register_component(n, charging_epoint_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_charging_epoint_number(n))
        
  if absorbing_epoint_config := config.get(CONF_ABSORBING_EPOINT):
        n = await number.new_number(
            absorbing_epoint_config, min_value=0.0, max_value=0.5, step=0.05
        )
        await cg.register_component(n, absorbing_epoint_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_absorbing_epoint_number(n))

  if floating_epoint_config := config.get(CONF_FLOATING_EPOINT):
        n = await number.new_number(
            floating_epoint_config, min_value=0, max_value=0.5, step=0.05
        )
        await cg.register_component(n, floating_epoint_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_floating_setpoint_number(n))     
        
  if starting_battery_voltage_config := config.get(CONF_STARTING_BATTERY_VOLTAGE):
        n = await number.new_number(
            starting_battery_voltage_config, min_value=50.0, max_value=60.0, step=0.1
        )
        await cg.register_component(n, starting_battery_voltage_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_starting_battery_voltage_number(n))

  if charged_battery_voltage_config := config.get(CONF_CHARGED_BATTERY_VOLTAGE):
        n = await number.new_number(
            charged_battery_voltage_config, min_value=50.0, max_value=60.0, step=0.1
        )
        await cg.register_component(n, charged_battery_voltage_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_charged_battery_voltage_number(n))

  if discharged_battery_voltage_config := config.get(CONF_DISCHARGED_BATTERY_VOLTAGE):
        n = await number.new_number(
            discharged_battery_voltage_config, min_value=50.0, max_value=60.0, step=0.1
        )
        await cg.register_component(n, discharged_battery_voltage_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_discharged_battery_voltage_number(n))  


  if kp_charging_config := config.get(CONF_KP_CHARGING):
        n = await number.new_number(
            kp_charging_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kp_charging_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_kp_charging_number(n)) 
  
  if ki_charging_config := config.get(CONF_KI_CHARGING):
        n = await number.new_number(
            ki_charging_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, ki_charging_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_ki_charging_number(n))

  if kd_charging_config := config.get(CONF_KD_CHARGING):
        n = await number.new_number(
            kd_charging_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kd_charging_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_kd_charging_number(n))
        
        
  if kp_discharging_config := config.get(CONF_KP_DISCHARGING):
        n = await number.new_number(
            kp_discharging_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kp_discharging_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_kp_discharging_number(n)) 
  
  if ki_discharging_config := config.get(CONF_KI_DISCHARGING):
        n = await number.new_number(
            ki_discharging_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, ki_discharging_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_ki_discharging_number(n))

  if kd_discharging_config := config.get(CONF_KD_DISCHARGING):
        n = await number.new_number(
            kd_discharging_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kd_discharging_config)
        await cg.register_parented(n, dualpid_component)
        cg.add(dualpid_component.set_kd_discharging_number(n))        


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





