import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    UNIT_DECIBEL_MILLIWATT,
    ICON_SIGNAL,
    ENTITY_CATEGORY_CONFIG,
)

from .. import CONF_RYLR999_BLE_ID, rylr999_ble_ns, RYLR999BLEComponent

DEPENDENCIES = ["rylr999_ble"]

CONF_TX_POWER = "tx_power"
CONF_RESTORE_VALUE = "restore_value"

RYLR999BLETxPowerNumber = rylr999_ble_ns.class_(
    "RYLR999BLETxPowerNumber", number.Number, cg.Component
)

# AT+CRFOP accepts 0-30 dBm on the RYLR999.
TX_POWER_MIN = 0
TX_POWER_MAX = 30
TX_POWER_STEP = 1

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RYLR999_BLE_ID): cv.use_id(RYLR999BLEComponent),
        cv.Optional(CONF_TX_POWER): number.number_schema(
            RYLR999BLETxPowerNumber,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            icon=ICON_SIGNAL,
            entity_category=ENTITY_CATEGORY_CONFIG,
        )
        .extend(
            {
                # Opt-in: when false (default) tx_power from the YAML is always used at boot.
                cv.Optional(CONF_RESTORE_VALUE, default=False): cv.boolean,
            }
        )
        .extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RYLR999_BLE_ID])

    if CONF_TX_POWER in config:
        n = await number.new_number(
            config[CONF_TX_POWER],
            min_value=TX_POWER_MIN,
            max_value=TX_POWER_MAX,
            step=TX_POWER_STEP,
        )
        await cg.register_component(n, config[CONF_TX_POWER])
        cg.add(n.set_parent(parent))
        cg.add(n.set_restore_value(config[CONF_TX_POWER][CONF_RESTORE_VALUE]))
        cg.add(parent.set_tx_power_number(n))
