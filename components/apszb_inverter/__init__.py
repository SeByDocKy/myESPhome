"""One APsystems (ZigBee) micro-inverter: YC600 / QS1 / DS3.

Declare one entry per inverter; each entry is attached to the ``apszb`` hub. The entities (sensor, text_sensor,
binary_sensor, number, button, output) are platforms of ``apszb_inverter`` and point to the inverter with
``inverter: <id>``.

    apszb_inverter:
      - id: inv1
        apszb_id: aps
        serial: "408000158215"
        type: yc600
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE

from ..apszb import (  # noqa: F401  (apszb_ns, APSInverter and CONF_INVERTER are re-used by the platforms)
    CONF_APSZB_ID,
    CONF_INVERTER,
    APSInverter,
    APSZBComponent,
    apszb_ns,
    hex12,
)

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["apszb"]
MULTI_CONF = True

CONF_SERIAL = "serial"
CONF_SHORT_ADDRESS = "short_address"
CONF_CALIBRATION = "calibration"
CONF_RATED_POWER = "rated_power"

INVERTER_TYPES = {"yc600": 0, "qs1": 1, "ds3": 2}
DEFAULT_RATED_POWER = {"yc600": 600, "qs1": 1200, "ds3": 730}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(APSInverter),
        cv.GenerateID(CONF_APSZB_ID): cv.use_id(APSZBComponent),
        cv.Required(CONF_SERIAL): hex12,
        cv.Required(CONF_TYPE): cv.enum(INVERTER_TYPES, lower=True),
        # normally learnt by the pairing procedure and stored in flash; can be forced here
        cv.Optional(CONF_SHORT_ADDRESS): cv.hex_uint16_t,
        # offset (W) added to the power limit sent to the inverter, as in the original firmware
        cv.Optional(CONF_CALIBRATION, default=0): cv.int_range(min=-100, max=100),
        # reference (W) of the `power_limit_percent` output; default depends on the type (yc600 600, qs1 1200, ds3 730)
        cv.Optional(CONF_RATED_POWER): cv.int_range(min=30, max=2000),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_APSZB_ID])
    inv = cg.new_Pvariable(config[CONF_ID])
    cg.add(inv.set_serial(config[CONF_SERIAL]))
    cg.add(inv.set_type(config[CONF_TYPE]))
    cg.add(inv.set_calibration(config[CONF_CALIBRATION]))
    cg.add(inv.set_rated_power(config.get(CONF_RATED_POWER, DEFAULT_RATED_POWER[config[CONF_TYPE]])))
    if CONF_SHORT_ADDRESS in config:
        cg.add(inv.set_short_address(config[CONF_SHORT_ADDRESS]))
    cg.add(hub.add_inverter(inv))
