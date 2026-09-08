import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import UNIT_PERCENT, UNIT_WATT

from .. import HMSComponent, hms_ns

CONF_HMS_ID = "hms_id"
CONF_POWER_PERCENT = "power_percent"
CONF_POWER_ABSOLUTE = "power_absolute"

DEPENDENCIES = ["hms"]

HMSPowerPercentNumber = hms_ns.class_("HMSPowerPercentNumber", number.Number)
HMSPowerAbsoluteNumber = hms_ns.class_("HMSPowerAbsoluteNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_ID): cv.use_id(HMSComponent),
        cv.Optional(CONF_POWER_PERCENT): number.number_schema(
            HMSPowerPercentNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:flash",
        ),
        cv.Optional(CONF_POWER_ABSOLUTE): number.number_schema(
            HMSPowerAbsoluteNumber,
            unit_of_measurement=UNIT_WATT,
            icon="mdi:flash",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_ID])

    if CONF_POWER_PERCENT in config:
        conf = config[CONF_POWER_PERCENT]
        n = await number.new_number(conf, min_value=0, max_value=100, step=1)
        cg.add(n.set_parent(hub))

    if CONF_POWER_ABSOLUTE in config:
        conf = config[CONF_POWER_ABSOLUTE]
        # Plage large par défaut : l'onduleur applique de toute façon son propre
        # plafond matériel. Ajuste max_value si tu veux une limite UI plus stricte.
        n = await number.new_number(conf, min_value=0, max_value=4000, step=1)
        cg.add(n.set_parent(hub))
