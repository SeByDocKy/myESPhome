import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, output, time

CODEOWNERS = ["@sebydocky"]
DEPENDENCIES = ["time"]
MULTI_CONF = True

hms_regulation_ns = cg.esphome_ns.namespace("hms_regulation")
HMS_REGULATIONComponent = hms_regulation_ns.class_("HMS_REGULATIONComponent", cg.Component)

from esphome.const import (
    CONF_ID,
)

CONF_HMS_REGULATION_ID = "hms_regulation_id"
CONF_INPUT_ID = "input_id"
CONF_OUTPUT_ID = 'output_id'


HMS_REGULATIONComponent_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_REGULATION_ID): cv.use_id(HMS_REGULATIONComponent),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
          cv.GenerateID(): cv.declare_id(HMS_REGULATIONComponent),
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
