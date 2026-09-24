import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_PERCENT

from .. import CONF_TSUNGEN3_ID, TSunGen3Component, tsungen3_ns

DEPENDENCIES = ["tsungen3"]

CONF_POWER_PERCENT = "power_percent"

TSunGen3PowerPercentNumber = tsungen3_ns.class_(
    "TSunGen3PowerPercentNumber", number.Number, cg.Parented.template(TSunGen3Component)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSUNGEN3_ID): cv.use_id(TSunGen3Component),
        cv.Optional(CONF_POWER_PERCENT): number.number_schema(
            TSunGen3PowerPercentNumber,
            icon="mdi:percent",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_PERCENT,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_TSUNGEN3_ID])
    if CONF_POWER_PERCENT in config:
        conf = config[CONF_POWER_PERCENT]
        var = await number.new_number(conf, min_value=0.0, max_value=100.0, step=1.0)
        await cg.register_parented(var, hub)
