import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID
from .. import ez1m_ns, EZ1MComponent, CONF_EZ1M_ID

DEPENDENCIES = ["ez1m"]

EZ1MOutput = ez1m_ns.class_(
    "EZ1MOutput", output.FloatOutput, cg.Parented.template(EZ1MComponent)
)

CONF_POWER_OUTPUT = "power_output"
CONF_MAX_POWER = "max_power"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EZ1M_ID): cv.use_id(EZ1MComponent),
        cv.Optional(CONF_POWER_OUTPUT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(EZ1MOutput),
                # 1.0 == max_power watts (the EZ1-M's hardware ceiling is 800 W).
                cv.Optional(CONF_MAX_POWER, default=800.0): cv.float_,
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EZ1M_ID])
    if CONF_POWER_OUTPUT in config:
        conf = config[CONF_POWER_OUTPUT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_max_power(conf[CONF_MAX_POWER]))
