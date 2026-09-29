import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import CONF_DEYEMI_ID, DeyeMiComponent, deyemi_ns

DEPENDENCIES = ["deyemi"]

CONF_POWER_PERCENT = "power_percent"

DeyeMiPowerPercentOutput = deyemi_ns.class_("DeyeMiPowerPercentOutput", output.FloatOutput,
                                                 cg.Parented.template(DeyeMiComponent))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYEMI_ID): cv.use_id(DeyeMiComponent),
        cv.Optional(CONF_POWER_PERCENT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(DeyeMiPowerPercentOutput),
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYEMI_ID])
    if CONF_POWER_PERCENT in config:
        conf = config[CONF_POWER_PERCENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        await cg.register_parented(var, hub)
