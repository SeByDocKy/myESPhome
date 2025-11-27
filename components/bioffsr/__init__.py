import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, output, time

CODEOWNERS = ["@sebydocky"]
DEPENDENCIES = ["time"]
MULTI_CONF = True

bioffsr_ns = cg.esphome_ns.namespace("bioffsr")
BIOFFSRComponent = bioffsr_ns.class_("BIOFFSRComponent", cg.Component)

from esphome.const import (
    CONF_ID,
)
CONF_BIOFFSR_ID = "bioffsr_id"
CONF_BATTERY_CURRENT_ID = 'battery_current_id'
CONF_BATTERY_VOLTAGE_ID = 'battery_voltage_id'
CONF_OUTPUT1_ID = 'output1_id'
CONF_OUTPUT2_ID = 'output2_id'
CONF_POWER_ID = 'power_id'

BIOFFSRComponent_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BIOFFSR_ID): cv.use_id(BIOFFSRComponent),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
          cv.GenerateID(): cv.declare_id(BIOFFSRComponent),
          cv.Required(CONF_BATTERY_CURRENT_ID): cv.use_id(sensor.Sensor),
          cv.Required(CONF_BATTERY_VOLTAGE_ID): cv.use_id(sensor.Sensor),     
          cv.Required(CONF_OUTPUT1_ID): cv.use_id(output.FloatOutput),
          cv.Required(CONF_OUTPUT2_ID): cv.use_id(output.FloatOutput),
          cv.Optional(CONF_POWER_ID): cv.use_id(sensor.Sensor), 
        }
    )
 )
 
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
	
    sens = await cg.get_variable(config[CONF_BATTERY_CURRENT_ID])
    cg.add(var.set_battery_current_sensor(sens))

    sens = await cg.get_variable(config[CONF_BATTERY_VOLTAGE_ID])
    cg.add(var.set_battery_voltage_sensor(sens))
	
    out = await cg.get_variable(config[CONF_OUTPUT1_ID])
    cg.add(var.set_device_output1(out))
    
    out = await cg.get_variable(config[CONF_OUTPUT2_ID])
    cg.add(var.set_device_output2(out))
        
    if CONF_POWER_ID in config:
        sens = await cg.get_variable(config[CONF_POWER_ID])
        cg.add(var.set_power_sensor(sens))