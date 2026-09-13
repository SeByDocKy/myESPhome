import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from esphome.components import cmt2300a as cmt2300a_ns_module

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

hms_ns = cg.esphome_ns.namespace("hms")
HMSComponent = hms_ns.class_("HMSComponent", cg.Component)

CONF_CMT2300A_ID = "cmt2300a_id"
CONF_SN = "sn"
CONF_DTU_SERIAL = "dtu_serial"
CONF_FREQUENCY_BAND = "frequency_band"
CONF_POLL_INTERVAL = "poll_interval"
CONF_REALTIME_TIMEOUT = "realtime_timeout"
CONF_POWER_CONTROL_TIMEOUT = "power_control_timeout"

FREQUENCY_BANDS = {
    "eu_860": 0,
    "us_900": 1,
}

_SERIAL_RE = re.compile(r"^[0-9A-Fa-f]{12}$")


def _validate_serial(value):
    value = cv.string_strict(value)
    if not _SERIAL_RE.match(value):
        raise cv.Invalid(
            "The serial number must be 12 hexadecimal characters (e.g. '1410A011xxxx')"
        )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HMSComponent),
        cv.GenerateID(CONF_CMT2300A_ID): cv.use_id(
            cmt2300a_ns_module.CMT2300AComponent
        ),
        cv.Required(CONF_SN): _validate_serial,
        cv.Optional(CONF_DTU_SERIAL): _validate_serial,
        cv.Optional(CONF_FREQUENCY_BAND, default="eu_860"): cv.enum(
            FREQUENCY_BANDS, lower=True
        ),
        cv.Optional(CONF_POLL_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        # Delays before giving up/retransmitting -- default identical to
        # OpenDTU's (RealTimeRunDataCommand::setTimeout(500),
        # ActivePowerControlCommand::setTimeout(2000)). Optional: only adjust
        # if real calibration (e.g. via the duty_cycle sensor) shows a benefit
        # for your installation.
        cv.Optional(
            CONF_REALTIME_TIMEOUT, default="500ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_POWER_CONTROL_TIMEOUT, default="2000ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    radio = await cg.get_variable(config[CONF_CMT2300A_ID])
    # The hub takes full control of the CMT2300A: register banks, FIFO,
    # Tx/Rx cadence specific to the Hoymiles protocol (incompatible with the
    # cmt2300a component's generic mode).
    cg.add(radio.set_external_mode(True))
    cg.add(var.set_radio(radio))

    cg.add(var.set_inverter_serial(int(config[CONF_SN], 16)))
    if CONF_DTU_SERIAL in config:
        cg.add(var.set_dtu_serial(int(config[CONF_DTU_SERIAL], 16)))

    cg.add(var.set_frequency_band(config[CONF_FREQUENCY_BAND]))
    cg.add(var.set_poll_interval(config[CONF_POLL_INTERVAL]))
    cg.add(var.set_realtime_timeout(config[CONF_REALTIME_TIMEOUT]))
    cg.add(var.set_power_control_timeout(config[CONF_POWER_CONTROL_TIMEOUT]))
