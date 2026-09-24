import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import CONF_TSUNGEN3_ID, TSunGen3Component, tsungen3_ns

DEPENDENCIES = ["tsungen3"]

CONF_POWER_PERCENT = "power_percent"

TSunGen3PowerPercentOutput = tsungen3_ns.class_(
    "TSunGen3PowerPercentOutput", output.FloatOutput, cg.Parented.template(TSunGen3Component)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSUNGEN3_ID): cv.use_id(TSunGen3Component),
        cv.Optional(CONF_POWER_PERCENT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(TSunGen3PowerPercentOutput),
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_TSUNGEN3_ID])
    if CONF_POWER_PERCENT in config:
        conf = config[CONF_POWER_PERCENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        await cg.register_parented(var, hub)
