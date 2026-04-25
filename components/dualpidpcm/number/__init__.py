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

DEPENDENCIES = ["dualpidpcm"]
ICON_SINE_WAVE = "mdi:sine-wave"

from .. import CONF_DUALPIDPCM_ID, DUALPIDPCMComponent, dualpidpcm_ns

SetpointNumber = dualpidpcm_ns.class_("SetpointNumber", number.Number, cg.Component)
StartingBatteryVoltageNumber = dualpidpcm_ns.class_("StartingBatteryVoltageNumber", number.Number, cg.Component)


KpNumber = dualpidpcm_ns.class_("KpNumber", number.Number, cg.Component)
KiNumber = dualpidpcm_ns.class_("KiNumber", number.Number, cg.Component)
KdNumber = dualpidpcm_ns.class_("KdNumber", number.Number, cg.Component)



# KpChargingNumber = dualpidpcm_ns.class_("KpChargingNumber", number.Number, cg.Component)
# KiChargingNumber = dualpidpcm_ns.class_("KiChargingNumber", number.Number, cg.Component)
# KdChargingNumber = dualpidpcm_ns.class_("KdChargingNumber", number.Number, cg.Component)

# KpDischargingNumber = dualpidpcm_ns.class_("KpDischargingNumber", number.Number, cg.Component)
# KiDischargingNumber = dualpidpcm_ns.class_("KiDischargingNumber", number.Number, cg.Component)
# KdDischargingNumber = dualpidpcm_ns.class_("KdDischargingNumber", number.Number, cg.Component)

OutputMinChargingNumber = dualpidpcm_ns.class_("OutputMinChargingNumber", number.Number, cg.Component)
OutputMaxChargingNumber = dualpidpcm_ns.class_("OutputMaxChargingNumber", number.Number, cg.Component)

OutputMinDischargingNumber = dualpidpcm_ns.class_("OutputMinDischargingNumber", number.Number, cg.Component)
OutputMaxDischargingNumber = dualpidpcm_ns.class_("OutputMaxDischargingNumber", number.Number, cg.Component)


CONF_SETPOINT = "setpoint"
CONF_STARTING_BATTERY_VOLTAGE = "starting_battery_voltage"


CONF_KP = "kp"
CONF_KI = "ki"
CONF_KD = "kd"

# CONF_KP_CHARGING = "kp_charging"
# CONF_KI_CHARGING = "ki_charging"
# CONF_KD_CHARGING = "kd_charging"

# CONF_KP_DISCHARGING = "kp_discharging"
# CONF_KI_DISCHARGING = "ki_discharging"
# CONF_KD_DISCHARGING = "kd_discharging"

CONF_OUTPUT_MIN_CHARGING = "output_min_charging"
CONF_OUTPUT_MAX_CHARGING = "output_max_charging"

CONF_OUTPUT_MIN_DISCHARGING = "output_min_discharging"
CONF_OUTPUT_MAX_DISCHARGING = "output_max_discharging"


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_DUALPIDPCM_ID): cv.use_id(DUALPIDPCMComponent),
    
    cv.Optional(CONF_SETPOINT): number.number_schema(
        SetpointNumber,
        device_class=DEVICE_CLASS_POWER,
        icon = ICON_POWER,
        unit_of_measurement=UNIT_WATT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_STARTING_BATTERY_VOLTAGE): number.number_schema(
        StartingBatteryVoltageNumber,
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

        
    # cv.Optional(CONF_KP_CHARGING): number.number_schema(
    #     KpChargingNumber,
    #     entity_category=ENTITY_CATEGORY_CONFIG
    # ).extend(cv.COMPONENT_SCHEMA),
    
    # cv.Optional(CONF_KI_CHARGING): number.number_schema(
    #     KiChargingNumber,
    #     entity_category=ENTITY_CATEGORY_CONFIG
    # ).extend(cv.COMPONENT_SCHEMA),
    
    # cv.Optional(CONF_KD_CHARGING): number.number_schema(
    #     KdChargingNumber,
    #     entity_category=ENTITY_CATEGORY_CONFIG
    # ).extend(cv.COMPONENT_SCHEMA),
    
    # cv.Optional(CONF_KP_DISCHARGING): number.number_schema(
    #     KpDischargingNumber,
    #     entity_category=ENTITY_CATEGORY_CONFIG
    # ).extend(cv.COMPONENT_SCHEMA),
    
    # cv.Optional(CONF_KI_DISCHARGING): number.number_schema(
    #     KiDischargingNumber,
    #     entity_category=ENTITY_CATEGORY_CONFIG
    # ).extend(cv.COMPONENT_SCHEMA),
    
    # cv.Optional(CONF_KD_DISCHARGING): number.number_schema(
    #     KdDischargingNumber,
    #     entity_category=ENTITY_CATEGORY_CONFIG
    # ).extend(cv.COMPONENT_SCHEMA),    

    cv.Optional(CONF_OUTPUT_MIN_CHARGING): number.number_schema(
        OutputMinChargingNumber,
        icon = ICON_PERCENT,
        unit_of_measurement=UNIT_PERCENT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_OUTPUT_MAX_CHARGING): number.number_schema(
        OutputMaxChargingNumber,
        icon = ICON_PERCENT,
        unit_of_measurement=UNIT_PERCENT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),

    cv.Optional(CONF_OUTPUT_MIN_DISCHARGING): number.number_schema(
        OutputMinDischargingNumber,
        icon = ICON_PERCENT,
        unit_of_measurement=UNIT_PERCENT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),
    
    cv.Optional(CONF_OUTPUT_MAX_DISCHARGING): number.number_schema(
        OutputMaxDischargingNumber,
        icon = ICON_PERCENT,
        unit_of_measurement=UNIT_PERCENT,
        entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(cv.COMPONENT_SCHEMA),                      
}

async def to_code(config):
  dualpidpcm_component = await cg.get_variable(config[CONF_DUALPIDPCM_ID])
  
  if setpoint_config := config.get(CONF_SETPOINT):
        n = await number.new_number(
            setpoint_config, min_value=-400, max_value=400, step=5
        )
        await cg.register_component(n, setpoint_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_setpoint_number(n))
      
  if starting_battery_voltage_config := config.get(CONF_STARTING_BATTERY_VOLTAGE):
        n = await number.new_number(
            starting_battery_voltage_config, min_value=50.0, max_value=60.0, step=0.1
        )
        await cg.register_component(n, starting_battery_voltage_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_starting_battery_voltage_number(n))

  if kp_config := config.get(CONF_KP):
        n = await number.new_number(
            kp_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kp_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_kp_number(n)) 
  
  if ki_config := config.get(CONF_KI):
        n = await number.new_number(
            ki_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, ki_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_ki_number(n))

  if kd_config := config.get(CONF_KD):
        n = await number.new_number(
            kd_config, min_value=0.0, max_value=10.0, step=0.1
        )
        await cg.register_component(n, kd_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_kd_number(n))



  # if kp_charging_config := config.get(CONF_KP_CHARGING):
  #       n = await number.new_number(
  #           kp_charging_config, min_value=0.0, max_value=10.0, step=0.1
  #       )
  #       await cg.register_component(n, kp_charging_config)
  #       await cg.register_parented(n, dualpidpcm_component)
  #       cg.add(dualpidpcm_component.set_kp_charging_number(n)) 
  
  # if ki_charging_config := config.get(CONF_KI_CHARGING):
  #       n = await number.new_number(
  #           ki_charging_config, min_value=0.0, max_value=10.0, step=0.1
  #       )
  #       await cg.register_component(n, ki_charging_config)
  #       await cg.register_parented(n, dualpidpcm_component)
  #       cg.add(dualpidpcm_component.set_ki_charging_number(n))

  # if kd_charging_config := config.get(CONF_KD_CHARGING):
  #       n = await number.new_number(
  #           kd_charging_config, min_value=0.0, max_value=10.0, step=0.1
  #       )
  #       await cg.register_component(n, kd_charging_config)
  #       await cg.register_parented(n, dualpidpcm_component)
  #       cg.add(dualpidpcm_component.set_kd_charging_number(n))
        
  # if kp_discharging_config := config.get(CONF_KP_DISCHARGING):
  #       n = await number.new_number(
  #           kp_discharging_config, min_value=0.0, max_value=10.0, step=0.1
  #       )
  #       await cg.register_component(n, kp_discharging_config)
  #       await cg.register_parented(n, dualpidpcm_component)
  #       cg.add(dualpidpcm_component.set_kp_discharging_number(n)) 
  
  # if ki_discharging_config := config.get(CONF_KI_DISCHARGING):
  #       n = await number.new_number(
  #           ki_discharging_config, min_value=0.0, max_value=10.0, step=0.1
  #       )
  #       await cg.register_component(n, ki_discharging_config)
  #       await cg.register_parented(n, dualpidpcm_component)
  #       cg.add(dualpidpcm_component.set_ki_discharging_number(n))

  # if kd_discharging_config := config.get(CONF_KD_DISCHARGING):
  #       n = await number.new_number(
  #           kd_discharging_config, min_value=0.0, max_value=10.0, step=0.1
  #       )
  #       await cg.register_component(n, kd_discharging_config)
  #       await cg.register_parented(n, dualpidpcm_component)
  #       cg.add(dualpidpcm_component.set_kd_discharging_number(n))        

  if output_min_charging_config := config.get(CONF_OUTPUT_MIN_CHARGING):
        n = await number.new_number(
            output_min_charging_config, min_value=0.0, max_value=100.0, step=1.0
        )
        await cg.register_component(n, output_min_charging_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_output_min_charging_number(n))

  if output_max_charging_config := config.get(CONF_OUTPUT_MAX_CHARGING):
        n = await number.new_number(
            output_max_charging_config, min_value=0.0, max_value=100.0, step=1.0
        )
        await cg.register_component(n, output_max_charging_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_output_max_charging_number(n))

  if output_min_discharging_config := config.get(CONF_OUTPUT_MIN_DISCHARGING):
        n = await number.new_number(
            output_min_discharging_config, min_value=0.0, max_value=100.0, step=1.0
        )
        await cg.register_component(n, output_min_discharging_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_output_min_discharging_number(n))

  if output_max_discharging_config := config.get(CONF_OUTPUT_MAX_DISCHARGING):
        n = await number.new_number(
            output_max_discharging_config, min_value=0.0, max_value=100.0, step=1.0
        )
        await cg.register_component(n, output_max_discharging_config)
        await cg.register_parented(n, dualpidpcm_component)
        cg.add(dualpidpcm_component.set_output_max_discharging_number(n))
