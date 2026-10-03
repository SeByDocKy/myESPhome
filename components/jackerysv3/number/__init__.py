import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MODE, ENTITY_CATEGORY_CONFIG, UNIT_PERCENT, UNIT_WATT

from .. import CONF_JACKERYSV3_ID, JackerySV3Hub, jackerysv3_ns

DEPENDENCIES = ["jackerysv3"]

JackerySV3Number = jackerysv3_ns.class_("JackerySV3Number", number.Number, cg.Component, cg.Parented.template(JackerySV3Hub))
NumberKind = jackerysv3_ns.enum("NumberKind", True)

# (config key, NumberKind member, min, max, step, unit, icon, entity category)
# The ranges are the ones of the official Home Assistant integration; the battery narrows them at run time
# (minSocChg / maxSocChg / minSocDischg / maxSocDischg).
NUMBERS = [
    ("soc_charge_limit", "SOC_CHARGE_LIMIT", 50, 100, 1, UNIT_PERCENT, "mdi:battery-arrow-up", ENTITY_CATEGORY_CONFIG),
    ("soc_discharge_limit", "SOC_DISCHARGE_LIMIT", 5, 49, 1, UNIT_PERCENT, "mdi:battery-arrow-down", ENTITY_CATEGORY_CONFIG),
    ("max_output_power", "MAX_OUTPUT_POWER", 0, 2500, 10, UNIT_WATT, "mdi:speedometer", ENTITY_CATEGORY_CONFIG),
]


def _schema(unit, icon, category):
    return number.number_schema(
        JackerySV3Number, unit_of_measurement=unit, icon=icon, entity_category=category
    ).extend({cv.Optional(CONF_MODE, default="SLIDER"): cv.enum(number.NUMBER_MODES, upper=True)})


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_JACKERYSV3_ID): cv.use_id(JackerySV3Hub),
        **{cv.Optional(key): _schema(unit, icon, cat) for key, _k, _lo, _hi, _st, unit, icon, cat in NUMBERS},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JACKERYSV3_ID])
    for key, kind, lo, hi, step, _unit, _icon, _cat in NUMBERS:
        if key in config:
            var = await number.new_number(config[key], min_value=float(lo), max_value=float(hi), step=float(step))
            await cg.register_component(var, config[key])
            await cg.register_parented(var, hub)
            cg.add(var.set_kind(getattr(NumberKind, kind)))
            cg.add(hub.add_number(getattr(NumberKind, kind), var))
