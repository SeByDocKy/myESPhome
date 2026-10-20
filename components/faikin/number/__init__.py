import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    UNIT_PERCENT,
)

from .. import CONF_FAIKIN_ID, FaikinComponent, faikin_ns

DEPENDENCIES = ["faikin"]

FaikinNumber = faikin_ns.class_(
    "FaikinNumber", number.Number, cg.Parented.template(FaikinComponent)
)

CONF_DEMAND = "demand"


def _validate_demand(config):
    if config[CONF_MIN_VALUE] >= config[CONF_MAX_VALUE]:
        raise cv.Invalid(f"{CONF_MIN_VALUE} must be lower than {CONF_MAX_VALUE}")
    return config


# Demand control ("power on demand") limits the conditioning power to a percentage; 100 means no limit.
# The step the unit really accepts is not documented, so it is configurable. Community reports mention
# a usable range of 40 to 100 %, with 5 % steps being the first thing to test on a given unit.
DEMAND_SCHEMA = cv.All(
    number.number_schema(
        FaikinNumber,
        unit_of_measurement=UNIT_PERCENT,
        icon="mdi:gauge",
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_MAX_VALUE, default=100): cv.int_range(min=0, max=100),
            cv.Optional(CONF_STEP, default=5): cv.int_range(min=1, max=50),
        }
    ),
    _validate_demand,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FAIKIN_ID): cv.use_id(FaikinComponent),
        cv.Optional(CONF_DEMAND): DEMAND_SCHEMA,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FAIKIN_ID])
    if conf := config.get(CONF_DEMAND):
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(var.set_parent(parent))
        cg.add(parent.set_demand_number(var))
