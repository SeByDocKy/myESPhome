import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import HMComponent, hm_ns

CONF_HM_ID = "hm_id"
CONF_POWER_LIMIT_PERCENT = "power_limit_percent"

DEPENDENCIES = ["hm"]

HMPowerLimitPercentOutput = hm_ns.class_(
    "HMPowerLimitPercentOutput", output.FloatOutput
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HM_ID): cv.use_id(HMComponent),
        cv.Optional(CONF_POWER_LIMIT_PERCENT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(HMPowerLimitPercentOutput),
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HM_ID])

    if CONF_POWER_LIMIT_PERCENT in config:
        conf = config[CONF_POWER_LIMIT_PERCENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        cg.add(var.set_parent(hub))
