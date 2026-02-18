"""
ESPHome number platform for RYLR998 LoRa module — contrôle de tx_power.

YAML usage:
  number:
    - platform: rylr998
      tx_power:
        name: ${name}_tx_power
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    UNIT_DECIBEL_MILLIWATT,
    ICON_SIGNAL,
    MODE_SLIDER,
    ENTITY_CATEGORY_CONFIG,
)

from .. import rylr998_ns, RYLR998Component

DEPENDENCIES = ["rylr998"]

# Défini localement — absent de certaines versions d'esphome.const
CONF_TX_POWER = "tx_power"
CONF_RYLR998_ID = "rylr998_id"

# ── Classe C++ ────────────────────────────────────────────────────────────────

RYLR998TxPowerNumber = rylr998_ns.class_(
    "RYLR998TxPowerNumber", number.Number
)

# ── Schema ────────────────────────────────────────────────────────────────────

# Le RYLR998 accepte 0 à 22 dBm par pas de 1 dBm (AT+CRFOP)
TX_POWER_MIN = 0
TX_POWER_MAX = 22
TX_POWER_STEP = 1

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RYLR998_ID): cv.use_id(RYLR998Component),
        cv.Optional(CONF_TX_POWER): number.number_schema(
            RYLR998TxPowerNumber,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            icon=ICON_SIGNAL,
            entity_category=ENTITY_CATEGORY_CONFIG,
            mode=MODE_SLIDER,
        ),
    }
)


# ── Code generation ───────────────────────────────────────────────────────────

async def to_code(config):
    parent = await cg.get_variable(config[CONF_RYLR998_ID])

    if CONF_TX_POWER in config:
        n = await number.new_number(
            config[CONF_TX_POWER],
            min_value=TX_POWER_MIN,
            max_value=TX_POWER_MAX,
            step=TX_POWER_STEP,
        )
        cg.add(n.set_parent(parent))
        cg.add(parent.set_tx_power_number(n))
