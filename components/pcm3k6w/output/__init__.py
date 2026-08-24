import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import CONF_PCM3K6W_ID, PCM3K6W_PLATFORM_SCHEMA, pcm3k6w_ns

DEPENDENCIES = ["canbus"]

PCM3K6WOutput = pcm3k6w_ns.class_("PCM3K6WOutput", output.FloatOutput)

CONF_MIN_CURRENT = "min_current"
CONF_MAX_CURRENT = "max_current"

# Kind indices MUST match the OutputKind enum order in pcm3k6w.h.
KIND_CHARGING_CURRENT, KIND_DISCHARGING_CURRENT = range(2)

# (config key, kind, default min amps, default max amps) - same bounds as the
# matching number/charging_current & number/discharging_current entities.
OUTPUTS = [
    ("charging_current_output", KIND_CHARGING_CURRENT, 1.0, 70.0),
    ("discharging_current_output", KIND_DISCHARGING_CURRENT, 1.0, 75.0),
]

CONFIG_SCHEMA = PCM3K6W_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PCM3K6WOutput),
                cv.Optional(CONF_MIN_CURRENT, default=min_default): cv.float_,
                cv.Optional(CONF_MAX_CURRENT, default=max_default): cv.float_,
            }
        )
        for key, _kind, min_default, max_default in OUTPUTS
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PCM3K6W_ID])
    for key, kind, _min_default, _max_default in OUTPUTS:
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await output.register_output(var, conf)
            await cg.register_parented(var, hub)
            cg.add(var.set_kind(kind))
            cg.add(var.set_range(conf[CONF_MIN_CURRENT], conf[CONF_MAX_CURRENT]))
