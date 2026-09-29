import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import CONF_DEYEGEN3_ID, DeyeGen3Component, deyegen3_ns

DEPENDENCIES = ["deyegen3"]

CONF_POWER_PERCENT = "power_percent"

DeyeGen3PowerPercentOutput = deyegen3_ns.class_("DeyeGen3PowerPercentOutput", output.FloatOutput,
                                                 cg.Parented.template(DeyeGen3Component))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYEGEN3_ID): cv.use_id(DeyeGen3Component),
        cv.Optional(CONF_POWER_PERCENT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(DeyeGen3PowerPercentOutput),
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYEGEN3_ID])
    if CONF_POWER_PERCENT in config:
        conf = config[CONF_POWER_PERCENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        await cg.register_parented(var, hub)
