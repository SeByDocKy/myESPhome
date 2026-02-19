import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_DECIBEL,
    ICON_SIGNAL,
)

from .. import CONF_RYLR998_ID, rylr998_ns, RYLR998Component

DEPENDENCIES = ["rylr998"]

CONF_RSSI = "rssi"
CONF_SNR  = "snr"
CONF_LAST_ERROR = "last_error"

CONF_RYLR998_ID = "rylr998_id"

# ── Schema ────────────────────────────────────────────────────────────────────

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RYLR998_ID): cv.use_id(RYLR998Component),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            icon=ICON_SIGNAL,
        ),
        cv.Optional(CONF_SNR): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
            icon=ICON_SIGNAL,
        ),
        cv.Optional(CONF_LAST_ERROR): sensor.sensor_schema(
            # Numéro d'erreur brut issu du tableau +ERR=N du protocole RYLR998.
            # 0 = pas d'erreur (valeur initiale).
            # Valeurs possibles : 1, 2, 4, 5, 10, 12, 13, 14, 15, 17, 18, 19, 20.
            icon="mdi:alert-circle-outline",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


# ── Code generation ───────────────────────────────────────────────────────────

async def to_code(config):
    parent = await cg.get_variable(config[CONF_RYLR998_ID])

    if CONF_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_RSSI])
        cg.add(parent.set_rssi_sensor(sens))

    if CONF_SNR in config:
        sens = await sensor.new_sensor(config[CONF_SNR])
        cg.add(parent.set_snr_sensor(sens))

    if CONF_LAST_ERROR in config:
        sens = await sensor.new_sensor(config[CONF_LAST_ERROR])
        cg.add(parent.set_last_error_sensor(sens))
