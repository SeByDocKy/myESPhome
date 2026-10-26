import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import CONF_DEYEMI_ID, DeyeMiComponent, deyemi_ns

DEPENDENCIES = ["deyemi"]

CONF_POWER_PERSISTENT = "power_persistent"
CONF_POWER_NON_PERSISTENT = "power_non_persistent"

# 0.0-1.0 -> 0-120 % of rated power (see deyemi.h).
# power_persistent: register 0x0028, stored in the inverter's non-volatile
# memory (survives restarts, wears flash).
DeyeMiPowerPersistentOutput = deyemi_ns.class_(
    "DeyeMiPowerPersistentOutput", output.FloatOutput, cg.Parented.template(DeyeMiComponent)
)
# power_non_persistent: register 0x0035, RAM only (not persisted, no wear).
DeyeMiPowerNonPersistentOutput = deyemi_ns.class_(
    "DeyeMiPowerNonPersistentOutput", output.FloatOutput, cg.Parented.template(DeyeMiComponent)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYEMI_ID): cv.use_id(DeyeMiComponent),
        cv.Optional(CONF_POWER_PERSISTENT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(DeyeMiPowerPersistentOutput)}
        ),
        cv.Optional(CONF_POWER_NON_PERSISTENT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(DeyeMiPowerNonPersistentOutput)}
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYEMI_ID])
    for key in (CONF_POWER_PERSISTENT, CONF_POWER_NON_PERSISTENT):
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await output.register_output(var, conf)
            await cg.register_parented(var, hub)
