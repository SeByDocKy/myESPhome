import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import (
    CONF_ID,
)

DEPENDENCIES = ["dmtcp"]

from .. import CONF_DMTCP_ID, DMTCPComponent, dmtcp_ns

DMTCPOutput = dmtcp_ns.class_("DMTCPOutput", output.FloatOutput, cg.Component)


CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DMTCPOutput),
        cv.GenerateID(CONF_DMTCP_ID): cv.use_id(DMTCPComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
  var = cg.new_Pvariable(config[CONF_ID])
  await cg.register_component(var, config)
  await output.register_output(var, config) 
  await cg.register_parented(var, config[CONF_DMTCP_ID])
          