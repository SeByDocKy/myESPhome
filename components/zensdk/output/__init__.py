import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import CONF_ZENSDK_ID, ZenSdkComponent, zensdk_ns

DEPENDENCIES = ["zensdk"]

ZenSdkChargeOutput = zensdk_ns.class_(
    "ZenSdkChargeOutput", output.FloatOutput, cg.Parented.template(ZenSdkComponent)
)
ZenSdkDischargeOutput = zensdk_ns.class_(
    "ZenSdkDischargeOutput", output.FloatOutput, cg.Parented.template(ZenSdkComponent)
)

CONF_CHARGE_POWER = "charge_power"
CONF_DISCHARGE_POWER = "discharge_power"

# 0.0 .. 1.0 maps to 0 .. max charge / discharge power of the configured model. The effective
# request is (discharge - charge), so a PID loop can drive each output independently.
OUTPUTS = [
    (CONF_CHARGE_POWER, ZenSdkChargeOutput),
    (CONF_DISCHARGE_POWER, ZenSdkDischargeOutput),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{
            cv.Optional(key): output.FLOAT_OUTPUT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(cls)})
            for key, cls in OUTPUTS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])
    for key, _cls in OUTPUTS:
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await output.register_output(var, conf)
            await cg.register_parented(var, hub)
