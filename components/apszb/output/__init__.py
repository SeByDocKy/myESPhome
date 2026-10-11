import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import APSInverter, CONF_INVERTER, apszb_ns

DEPENDENCIES = ["apszb"]

CONF_POWER_LIMIT_PERCENT = "power_limit_percent"

APSPowerLimitPercentOutput = apszb_ns.class_(
    "APSPowerLimitPercentOutput", output.FloatOutput, cg.Parented.template(APSInverter)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_INVERTER): cv.use_id(APSInverter),
        # 0.0 .. 1.0 = 0 .. 100 % of the rated power of the inverter (see `rated_power` of the inverter)
        cv.Optional(CONF_POWER_LIMIT_PERCENT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(APSPowerLimitPercentOutput),
            }
        ),
    }
)


async def to_code(config):
    inv = await cg.get_variable(config[CONF_INVERTER])
    if CONF_POWER_LIMIT_PERCENT in config:
        conf = config[CONF_POWER_LIMIT_PERCENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        await cg.register_parented(var, inv)
