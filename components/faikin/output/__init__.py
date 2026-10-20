import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_STEP

from .. import CONF_FAIKIN_ID, FaikinComponent, faikin_ns

DEPENDENCIES = ["faikin"]

FaikinOutput = faikin_ns.class_(
    "FaikinOutput", output.FloatOutput, cg.Component, cg.Parented.template(FaikinComponent)
)

CONF_MIN_DEMAND = "min_demand"
CONF_MAX_DEMAND = "max_demand"


def _validate(config):
    if config[CONF_MIN_DEMAND] >= config[CONF_MAX_DEMAND]:
        raise cv.Invalid(f"{CONF_MIN_DEMAND} must be lower than {CONF_MAX_DEMAND}")
    return config


# Float output for the demand control: 0.0 -> min_demand %, 1.0 -> max_demand %.
CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(FaikinOutput),
            cv.GenerateID(CONF_FAIKIN_ID): cv.use_id(FaikinComponent),
            cv.Optional(CONF_MIN_DEMAND, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_MAX_DEMAND, default=100): cv.int_range(min=0, max=100),
            cv.Optional(CONF_STEP, default=5): cv.int_range(min=1, max=50),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    parent = await cg.get_variable(config[CONF_FAIKIN_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_min_demand(config[CONF_MIN_DEMAND]))
    cg.add(var.set_max_demand(config[CONF_MAX_DEMAND]))
    cg.add(var.set_step(config[CONF_STEP]))
