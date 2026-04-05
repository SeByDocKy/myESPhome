import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, output, switch, time

CODEOWNERS = ["@sebydocky"]
DEPENDENCIES = ["time"]
MULTI_CONF = True

dualpidpcm_ns = cg.esphome_ns.namespace("dualpidpcm")
DUALPIDPCMComponent = dualpidpcm_ns.class_("DUALPIDPCMComponent", cg.Component)

from esphome.const import (
    CONF_ID,
)
CONF_DUALPIDPCM_ID = "dualpidpcm_id"
CONF_INPUT_ID = 'input_id'
CONF_BATTERY_VOLTAGE_ID = 'battery_voltage_id'
CONF_CHARGING_OUTPUT_ID = 'charging_output_id'
CONF_DISCHARGING_OUTPUT_ID = 'discharging_output_id'
CONF_DISCHARGE_CHARGE_SWITCH_ID = 'discharge_charge_switch_id'
CONF_ONOFF_SWITCH_ID = 'onoff_switch_id'



DUALPIDPCMComponent_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DUALPIDPCM_ID): cv.use_id(DUALPIDPCMComponent),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
          cv.GenerateID(): cv.declare_id(DUALPIDPCMComponent),
          cv.Required(CONF_INPUT_ID): cv.use_id(sensor.Sensor),
          cv.Required(CONF_BATTERY_VOLTAGE_ID): cv.use_id(sensor.Sensor),     
          cv.Required(CONF_CHARGING_OUTPUT_ID): cv.use_id(output.FloatOutput),
          cv.Required(CONF_DISCHARGING_OUTPUT_ID): cv.use_id(output.FloatOutput),
		  cv.Required(CONF_DISCHARGE_CHARGE_SWITCH_ID): cv.use_id(switch.Switch),
          cv.Required(CONF_ONOFF_SWITCH_ID): cv.use_id(switch.Switch),
        }
    )
 )
 
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
	
    sens = await cg.get_variable(config[CONF_INPUT_ID])
    cg.add(var.set_input_sensor(sens))

    sens = await cg.get_variable(config[CONF_BATTERY_VOLTAGE_ID])
    cg.add(var.set_battery_voltage_sensor(sens))
	
    out = await cg.get_variable(config[CONF_CHARGING_OUTPUT_ID])
    cg.add(var.set_device_charging_output(out))
    
    out = await cg.get_variable(config[CONF_DISCHARGING_OUTPUT_ID])
    cg.add(var.set_device_discharging_output(out))
	
    if CONF_DISCHARGE_CHARGE_SWITCH_ID in config:
      sw = await cg.get_variable(config[CONF_DISCHARGE_CHARGE_SWITCH_ID])
      cg.add(var.set_discharge_charge_switch(sw))
      
    if CONF_ONOFF_SWITCH_ID in config:
      sw = await cg.get_variable(config[CONF_ONOFF_SWITCH_ID])
      cg.add(var.set_onoff_switch(sw))  
		
 
