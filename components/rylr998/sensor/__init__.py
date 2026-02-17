"""
ESPHome sensor platform for RYLR998 LoRa module.

YAML usage:
  sensor:
    - platform: rylr998
      rssi:
        name: ${name}_rssi
      snr:
        name: ${name}_snr
"""

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

# Import the parent component namespace
from .. import rylr998_ns, RYLR998Component, CONF_RYLR998_ID

DEPENDENCIES = ["rylr998"]

# CONF_RSSI and CONF_SNR may not exist in all ESPHome versions — define them locally
CONF_RSSI = "rssi"
CONF_SNR  = "snr"

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
