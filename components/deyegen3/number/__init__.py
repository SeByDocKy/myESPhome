import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_PERCENT

from .. import CONF_DEYEGEN3_ID, DeyeGen3Component, deyegen3_ns

DEPENDENCIES = ["deyegen3"]

CONF_POWER_PERCENT = "power_percent"

DeyeGen3PowerPercentNumber = deyegen3_ns.class_("DeyeGen3PowerPercentNumber", number.Number,
                                                 cg.Parented.template(DeyeGen3Component))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYEGEN3_ID): cv.use_id(DeyeGen3Component),
        cv.Optional(CONF_POWER_PERCENT): number.number_schema(
            DeyeGen3PowerPercentNumber,
            icon="mdi:percent",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_PERCENT,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYEGEN3_ID])
    if CONF_POWER_PERCENT in config:
        conf = config[CONF_POWER_PERCENT]
        var = await number.new_number(conf, min_value=0.0, max_value=100.0, step=1.0)
        await cg.register_parented(var, hub)
