import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import UNIT_PERCENT

from .. import HMSWComponent, hmsw_ns

CONF_HMSW_ID = "hmsw_id"
# Named "persistent", not just "power_percent": unlike hm:/hms:, no
# non-persistent (RAM-only) variant of this command is known for HMS-XXXXW
# -- every write hits the inverter's EEPROM. See README.md before wiring
# this into a fast control loop. Matches the output: platform's key
# (persistent_power_percent) -- same command, same name, one is a
# number: and the other a float output:.
CONF_PERSISTENT_POWER_PERCENT = "persistent_power_percent"

DEPENDENCIES = ["hmsw"]

HMSWPersistentPowerPercentNumber = hmsw_ns.class_(
    "HMSWPersistentPowerPercentNumber", number.Number
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMSW_ID): cv.use_id(HMSWComponent),
        cv.Optional(CONF_PERSISTENT_POWER_PERCENT): number.number_schema(
            HMSWPersistentPowerPercentNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:flash",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMSW_ID])

    if CONF_PERSISTENT_POWER_PERCENT in config:
        conf = config[CONF_PERSISTENT_POWER_PERCENT]
        n = await number.new_number(conf, min_value=0, max_value=100, step=1)
        cg.add(n.set_parent(hub))
