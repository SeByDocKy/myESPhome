import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_PERCENT

from .. import CONF_DEYEMI_ID, DeyeMiComponent, deyemi_ns

DEPENDENCIES = ["deyemi"]

CONF_POWER_PERCENT = "power_percent"

DeyeMiPowerPercentNumber = deyemi_ns.class_("DeyeMiPowerPercentNumber", number.Number,
                                                 cg.Parented.template(DeyeMiComponent))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYEMI_ID): cv.use_id(DeyeMiComponent),
        cv.Optional(CONF_POWER_PERCENT): number.number_schema(
            DeyeMiPowerPercentNumber,
            icon="mdi:percent",
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_PERCENT,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYEMI_ID])
    if CONF_POWER_PERCENT in config:
        conf = config[CONF_POWER_PERCENT]
        var = await number.new_number(conf, min_value=0.0, max_value=100.0, step=1.0)
        await cg.register_parented(var, hub)
