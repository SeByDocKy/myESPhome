import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, output, switch, time

CODEOWNERS = ["@sebydocky"]
DEPENDENCIES = ["time"]
MULTI_CONF = True

dualpid_ns = cg.esphome_ns.namespace("dualpid")
DUALPIDComponent = dualpid_ns.class_("DUALPIDComponent", cg.Component)

from esphome.const import (
    CONF_ID,
)
CONF_DUALPID_ID = "dualpid_id"
CONF_INPUT_ID = 'input_id'
CONF_BATTERY_VOLTAGE_ID = 'battery_voltage_id'
CONF_CHARGING_OUTPUT_ID = 'charging_output_id'
CONF_DISCHARGING_OUTPUT_ID = 'discharging_output_id'
# CONF_POWER_ID = 'power_id'
CONF_R48_GENERAL_SWITCH_ID = 'r48_general_switch_id'

# PidUpdateAction = offsr_ns.class_('PidUpdateAction', automation.Action)

DUALPIDComponent_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DUALPID_ID): cv.use_id(DUALPIDComponent),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
          cv.GenerateID(): cv.declare_id(DUALPIDComponent),
          cv.Required(CONF_INPUT_ID): cv.use_id(sensor.Sensor),
          cv.Required(CONF_BATTERY_VOLTAGE_ID): cv.use_id(sensor.Sensor),     
          cv.Required(CONF_CHARGING_OUTPUT_ID): cv.use_id(output.FloatOutput),
          cv.Required(CONF_DISCHARGING_OUTPUT_ID): cv.use_id(output.FloatOutput),
		  cv.Optional(CONF_R48_GENERAL_SWITCH_ID): cv.use_id(switch.Switch),	
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

    sw = await cg.get_variable(config[CONF_R48_GENERAL_SWITCH_ID])
    cg.add(var.set_r48_general_switch(sw))

