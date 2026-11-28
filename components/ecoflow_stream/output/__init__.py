import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID
import esphome.final_validate as fv

from .. import (
    CONF_CHARGE_SIGN,
    CONF_ECOFLOW_STREAM_ID,
    DOMAIN,
    ECOFLOW_STREAM_PLATFORM_SCHEMA,
    EcoflowStreamHub,
    ecoflow_stream_ns,
)

DEPENDENCIES = ["ecoflow_stream"]

EcoflowStreamChargeOutput = ecoflow_stream_ns.class_(
    "EcoflowStreamChargeOutput", output.FloatOutput, cg.Parented.template(EcoflowStreamHub)
)
EcoflowStreamDischargeOutput = ecoflow_stream_ns.class_(
    "EcoflowStreamDischargeOutput", output.FloatOutput, cg.Parented.template(EcoflowStreamHub)
)

# 0.0 .. 1.0 maps to 0 .. maximum charge / discharge power. The effective request is (discharge - charge).
OUTPUTS = [
    ("charge_power", EcoflowStreamChargeOutput),
    ("discharge_power", EcoflowStreamDischargeOutput),
]

CONFIG_SCHEMA = ECOFLOW_STREAM_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): output.FLOAT_OUTPUT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(cls)})
        for key, cls in OUTPUTS
    }
)


def _final_validate(config):
    hub_id = config[CONF_ECOFLOW_STREAM_ID]
    for hub in fv.full_config.get().get(DOMAIN, []):
        if hub[CONF_ID].id == hub_id.id and CONF_CHARGE_SIGN not in hub:
            raise cv.Invalid(
                f"the output platform needs '{CONF_CHARGE_SIGN}: negative' or 'positive' in the ecoflow_stream hub "
                "(the sign of the target power that charges the battery: check it on your device, see the README)"
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ECOFLOW_STREAM_ID])
    for key, _cls in OUTPUTS:
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await output.register_output(var, conf)
            await cg.register_parented(var, hub)
            cg.add(hub.enable_outputs())
