import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import HMSComponent, hms_ns

CONF_HMS_ID = "hms_id"
CONF_POWER_LIMIT_PERCENT = "power_limit_percent"

DEPENDENCIES = ["hms"]

HMSPowerLimitPercentOutput = hms_ns.class_(
    "HMSPowerLimitPercentOutput", output.FloatOutput
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_ID): cv.use_id(HMSComponent),
        cv.Optional(CONF_POWER_LIMIT_PERCENT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(HMSPowerLimitPercentOutput),
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_ID])

    if CONF_POWER_LIMIT_PERCENT in config:
        conf = config[CONF_POWER_LIMIT_PERCENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        cg.add(var.set_parent(hub))
