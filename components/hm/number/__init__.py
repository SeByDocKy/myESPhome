import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import UNIT_PERCENT, UNIT_WATT

from .. import HMComponent, hm_ns

CONF_HM_ID = "hm_id"
CONF_POWER_PERCENT = "power_percent"
CONF_POWER_ABSOLUTE = "power_absolute"

DEPENDENCIES = ["hm"]

HMPowerPercentNumber = hm_ns.class_("HMPowerPercentNumber", number.Number)
HMPowerAbsoluteNumber = hm_ns.class_("HMPowerAbsoluteNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HM_ID): cv.use_id(HMComponent),
        cv.Optional(CONF_POWER_PERCENT): number.number_schema(
            HMPowerPercentNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:flash",
        ),
        cv.Optional(CONF_POWER_ABSOLUTE): number.number_schema(
            HMPowerAbsoluteNumber,
            unit_of_measurement=UNIT_WATT,
            icon="mdi:flash",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HM_ID])

    if CONF_POWER_PERCENT in config:
        conf = config[CONF_POWER_PERCENT]
        # min_value=2, pas 0 : à 0% l'onduleur arrête purement et simplement de
        # produire (cohérent avec reset_to_output_min, voir button/__init__.py).
        n = await number.new_number(conf, min_value=2, max_value=100, step=1)
        cg.add(n.set_parent(hub))

    if CONF_POWER_ABSOLUTE in config:
        conf = config[CONF_POWER_ABSOLUTE]
        n = await number.new_number(conf, min_value=0, max_value=4000, step=1)
        cg.add(n.set_parent(hub))
