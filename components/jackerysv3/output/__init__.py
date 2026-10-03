import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_JACKERYSV3_ID, JackerySV3Hub, jackerysv3_ns

DEPENDENCIES = ["jackerysv3"]

JackerySV3Output = jackerysv3_ns.class_("JackerySV3Output", output.FloatOutput, cg.Parented.template(JackerySV3Hub))
NumberKind = jackerysv3_ns.enum("NumberKind", True)

# (config key, NumberKind member, min, max, step). 0.0 -> min, 1.0 -> max. Same range as the `number` platform.
# Only the maximum output power is exposed: the SOC limits are settings, not something a control loop drives.
OUTPUTS = [
    ("max_output_power", "MAX_OUTPUT_POWER", 0, 2500, 10),  # W, maximum output power of the grid-tied port
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_JACKERYSV3_ID): cv.use_id(JackerySV3Hub),
        **{
            cv.Optional(key): output.FLOAT_OUTPUT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(JackerySV3Output)})
            for key, _kind, _lo, _hi, _step in OUTPUTS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JACKERYSV3_ID])
    for key, kind, lo, hi, step in OUTPUTS:
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await output.register_output(var, conf)
            await cg.register_parented(var, hub)
            cg.add(var.set_kind(getattr(NumberKind, kind)))
            cg.add(var.set_range(float(lo), float(hi), float(step)))
