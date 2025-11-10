import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import (
    CONF_ID,
)

from .. import EmersonR48Component, emerson_r48_ns, CONF_EMERSON_R48_ID

CONF_MAX_OUTPUT_CURRENT = "max_output_current"

EmersonR48MaxCurrentOutput = emerson_r48_ns.class_(
    "EmersonR48MaxCurrentOutput", output.FloatOutput, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MAX_OUTPUT_CURRENT): output.FLOAT_OUTPUT_SCHEMA.extend({
              cv.Required(CONF_ID): cv.declare_id(EmersonR48MaxCurrentOutput),
            }),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_EMERSON_R48_ID])
    if config[CONF_MAX_OUTPUT_CURRENT]:
        conf = config[CONF_MAX_OUTPUT_CURRENT]
        out = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(out, conf)
        cg.add(out.set_parent(hub))
        