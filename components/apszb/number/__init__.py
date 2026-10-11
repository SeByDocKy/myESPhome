import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    DEVICE_CLASS_POWER,
    UNIT_WATT,
)

from .. import APSInverter, CONF_INVERTER, apszb_ns

DEPENDENCIES = ["apszb"]

CONF_MAX_POWER = "max_power"

APSMaxPowerNumber = apszb_ns.class_("APSMaxPowerNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_INVERTER): cv.use_id(APSInverter),
        # power limit (throttling) of the inverter, read back from the inverter after each change
        cv.Optional(CONF_MAX_POWER): number.number_schema(
            APSMaxPowerNumber,
            unit_of_measurement=UNIT_WATT,
            device_class=DEVICE_CLASS_POWER,
            icon="mdi:speedometer",
        ).extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=30): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=800): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.positive_float,
            }
        ),
    }
)


async def to_code(config):
    inv = await cg.get_variable(config[CONF_INVERTER])
    if CONF_MAX_POWER in config:
        conf = config[CONF_MAX_POWER]
        num = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await cg.register_parented(num, inv)
        cg.add(inv.set_max_power_number(num))
