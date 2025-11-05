import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, output, time

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["time"]
MULTI_CONF = True

minipid_ns = cg.esphome_ns.namespace("minipid")
MINIPIDComponent = minipid_ns.class_("MINIPIDComponent", cg.Component)

from esphome.const import (
    CONF_ID,
)

CONF_MINIPID_ID = "minipid_id"
CONF_INPUT_ID = "input_id"
CONF_OUTPUT_ID = 'output_id'

HMS_REGULATIONComponent_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MINIPID_ID): cv.use_id(MINIPIDComponent),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
          cv.GenerateID(): cv.declare_id(MINIPIDComponent),
          cv.Required(CONF_INPUT_ID): cv.use_id(sensor.Sensor),
          cv.Required(CONF_OUTPUT_ID): cv.use_id(output.FloatOutput),
        }
    )
 )
 
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
	
    sens = await cg.get_variable(config[CONF_INPUT_ID])
    cg.add(var.set_input_sensor(sens))
	
    out = await cg.get_variable(config[CONF_OUTPUT_ID])
    cg.add(var.set_device_output(out))