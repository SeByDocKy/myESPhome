import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_ANKER_SOLIX_ID, ANKER_SOLIX_PLATFORM_SCHEMA, AnkerSolixHub, anker_solix_ns

DEPENDENCIES = ["anker_solix"]

AnkerChargeOutput = anker_solix_ns.class_(
    "AnkerChargeOutput", output.FloatOutput, cg.Parented.template(AnkerSolixHub)
)
AnkerDischargeOutput = anker_solix_ns.class_(
    "AnkerDischargeOutput", output.FloatOutput, cg.Parented.template(AnkerSolixHub)
)

CONF_CHARGE_POWER = "charge_power"
CONF_DISCHARGE_POWER = "discharge_power"

# 0.0 .. 1.0 maps to 0 .. max charge / discharge power of the battery (as it reports it, or max_charge_power /
# max_discharge_power of the hub). The effective request is (discharge - charge).
OUTPUTS = [
    (CONF_CHARGE_POWER, AnkerChargeOutput),
    (CONF_DISCHARGE_POWER, AnkerDischargeOutput),
]

CONFIG_SCHEMA = ANKER_SOLIX_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): output.FLOAT_OUTPUT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(cls)})
        for key, cls in OUTPUTS
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ANKER_SOLIX_ID])
    for key, _cls in OUTPUTS:
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await output.register_output(var, conf)
            await cg.register_parented(var, hub)
            cg.add(hub.enable_power_limits())
