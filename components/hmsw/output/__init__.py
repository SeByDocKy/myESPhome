import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

from .. import HMSWComponent, hmsw_ns

CONF_HMSW_ID = "hmsw_id"
# Named "persistent", not "power_percent": unlike hm:/hms:, no
# non-persistent (RAM-only) variant of this command is known for HMS-XXXXW --
# every write hits the inverter's EEPROM. See README.md before wiring this
# into a fast control loop.
CONF_PERSISTENT_POWER_PERCENT = "persistent_power_percent"

DEPENDENCIES = ["hmsw"]

HMSWPersistentPowerPercentOutput = hmsw_ns.class_(
    "HMSWPersistentPowerPercentOutput", output.FloatOutput
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMSW_ID): cv.use_id(HMSWComponent),
        cv.Optional(
            CONF_PERSISTENT_POWER_PERCENT
        ): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(HMSWPersistentPowerPercentOutput),
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMSW_ID])

    if CONF_PERSISTENT_POWER_PERCENT in config:
        conf = config[CONF_PERSISTENT_POWER_PERCENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        cg.add(var.set_parent(hub))
