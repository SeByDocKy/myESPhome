import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from esphome.components import nrf24l01 as nrf24l01_ns_module

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

hm_ns = cg.esphome_ns.namespace("hm")
HMComponent = hm_ns.class_("HMComponent", cg.Component)

CONF_NRF24L01_ID = "nrf24l01_id"
CONF_SN = "sn"
CONF_DTU_SERIAL = "dtu_serial"
CONF_POLL_INTERVAL = "poll_interval"
CONF_REALTIME_TIMEOUT = "realtime_timeout"
CONF_POWER_CONTROL_TIMEOUT = "power_control_timeout"

_SERIAL_RE = re.compile(r"^[0-9A-Fa-f]{12}$")


def _validate_serial(value):
    value = cv.string_strict(value)
    if not _SERIAL_RE.match(value):
        raise cv.Invalid(
            "The serial number must be 12 hexadecimal characters (e.g. '112183001234')"
        )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HMComponent),
        cv.GenerateID(CONF_NRF24L01_ID): cv.use_id(
            nrf24l01_ns_module.NRF24Component
        ),
        cv.Required(CONF_SN): _validate_serial,
        cv.Optional(CONF_DTU_SERIAL): _validate_serial,
        cv.Optional(CONF_POLL_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        # Délais avant abandon/retransmission -- mêmes valeurs par défaut
        # qu'OpenDTU, optionnels (voir hms: pour la même logique).
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

    radio = await cg.get_variable(config[CONF_NRF24L01_ID])
    # Le hub prend le contrôle total du nRF24 : data rate/CRC/largeur d'adresse
    # fixes imposés par le protocole Hoymiles NRF, hop de canal continu --
    # incompatible avec le mode générique du composant nrf24l01.
    cg.add(radio.set_external_mode(True))
    cg.add(var.set_radio(radio))

    cg.add(var.set_inverter_serial(int(config[CONF_SN], 16)))
    if CONF_DTU_SERIAL in config:
        cg.add(var.set_dtu_serial(int(config[CONF_DTU_SERIAL], 16)))

    cg.add(var.set_poll_interval(config[CONF_POLL_INTERVAL]))
    cg.add(var.set_realtime_timeout(config[CONF_REALTIME_TIMEOUT]))
    cg.add(var.set_power_control_timeout(config[CONF_POWER_CONTROL_TIMEOUT]))
