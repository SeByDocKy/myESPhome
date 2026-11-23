import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, esp32_ble_tracker
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    CONF_SERVICE_UUID,
    CONF_CHARACTERISTIC_UUID,
)

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["ble_client"]
CODEOWNERS = ["@SeByDocKy", "@claude"]
MULTI_CONF = True

# Some modules (including the RYLR999 used in practice) expose TWO separate
# characteristics in the same service - one for writing (READ | WRITE_NO_RESPONSE)
# and one for notifications only (NOTIFY) - whereas the REYAX documentation
# describes a single characteristic combining both. This field is optional: when
# omitted, characteristic_uuid is also used for notifications.
CONF_NOTIFY_CHARACTERISTIC_UUID = "notify_characteristic_uuid"
CONF_RYLR999_BLE_ID = "rylr999_ble_id"
CONF_ADDRESS = "address"
CONF_SPREADING_FACTOR = "spreading_factor"
CONF_SIGNAL_BANDWIDTH = "signal_bandwidth"
CONF_CODING_RATE = "coding_rate"
CONF_PREAMBLE_LENGTH = "preamble_length"
CONF_NETWORK_ID = "network_id"
CONF_TX_POWER = "tx_power"
CONF_AIR_TIME = "air_time"
CONF_MIN_TX_INTERVAL = "min_tx_interval"

rylr999_ble_ns = cg.esphome_ns.namespace("rylr999_ble")

RYLR999BLEComponent = rylr999_ble_ns.class_(
    "RYLR999BLEComponent", cg.Component, ble_client.BLEClientNode
)
# Observer interface used by the packet_transport platform to receive the decoded
# packets from "+RCV=..." lines (see rylr999_ble.h).
RYLR999BLEListener = rylr999_ble_ns.class_("RYLR999BLEListener")


def validate_bandwidth(value):
    value = cv.frequency(value)
    if value not in (125000, 250000, 500000):
        raise cv.Invalid("Bandwidth must be 125kHz, 250kHz or 500kHz")
    return value


def validate_spreading_factor(value):
    value = cv.int_(value)
    if value < 5 or value > 11:
        raise cv.Invalid("Spreading factor must be between 5 and 11")
    return value


# Spreading factors accepted by AT+PARAMETER for each bandwidth (REYAX AT command guide).
_SF_RANGE_BY_BANDWIDTH = {125000: (7, 9), 250000: (7, 10), 500000: (7, 11)}


def validate_coding_rate(value):
    value = cv.int_(value)
    if value < 1 or value > 4:
        raise cv.Invalid("Coding rate must be between 1 and 4")
    return value


def validate_network_id(value):
    value = cv.int_(value)
    valid_ids = list(range(3, 16)) + [18]
    if value not in valid_ids:
        raise cv.Invalid("Network ID must be 3-15 or 18")
    return value


def _validate_config(config):
    # preamble_length can only be changed (4-24) with network_id: 18, otherwise it must stay at 12
    # (same constraint as AT+PARAMETER in the REYAX guide).
    if config[CONF_NETWORK_ID] != 18 and config[CONF_PREAMBLE_LENGTH] != 12:
        raise cv.Invalid(
            "preamble_length can only be changed (4-24) when network_id: 18. "
            "For any other network_id it must stay at 12 (default)."
        )
    # Spreading factor ranges listed in the guide for each bandwidth.
    lo, hi = _SF_RANGE_BY_BANDWIDTH[config[CONF_SIGNAL_BANDWIDTH]]
    sf = config[CONF_SPREADING_FACTOR]
    if not lo <= sf <= hi:
        # The guide is ambiguous about SF5/SF6, so this is a warning rather than an error.
        _LOGGER.warning(
            "rylr999_ble: spreading_factor %s is outside %s-%s, the range listed in the REYAX guide for "
            "signal_bandwidth %skHz; the module may refuse AT+PARAMETER",
            sf,
            lo,
            hi,
            int(config[CONF_SIGNAL_BANDWIDTH]) // 1000,
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RYLR999BLEComponent),
            cv.Required(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
            cv.Required(CONF_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_NOTIFY_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=65535),
            cv.Optional(CONF_FREQUENCY, default=915000000): cv.frequency,
            cv.Optional(CONF_SPREADING_FACTOR, default=9): validate_spreading_factor,
            cv.Optional(CONF_SIGNAL_BANDWIDTH, default=125000): validate_bandwidth,
            cv.Optional(CONF_CODING_RATE, default=1): validate_coding_rate,
            cv.Optional(CONF_PREAMBLE_LENGTH, default=12): cv.int_range(min=4, max=24),
            cv.Optional(CONF_NETWORK_ID, default=18): validate_network_id,
            # RYLR999: power amplifier up to 30 dBm (22 dBm max on the RYLR998).
            cv.Optional(CONF_TX_POWER, default=22): cv.int_range(min=0, max=30),
            # true (default): the guard time after each AT+SEND is the computed air time of that
            # packet. false: fixed 600 ms (legacy behaviour).
            cv.Optional(CONF_AIR_TIME, default=True): cv.boolean,
            # Optional extra floor on the spacing between two AT+SEND. Normally not needed: the
            # computed air time already spaces the packets. Packets wait in the TX queue, they are
            # never dropped because of this setting.
            cv.Optional(
                CONF_MIN_TX_INTERVAL, default="0ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA),
    _validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    service_uuid = config[CONF_SERVICE_UUID]
    if len(service_uuid) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(var.set_service_uuid16(esp32_ble_tracker.as_hex(service_uuid)))
    elif len(service_uuid) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(var.set_service_uuid32(esp32_ble_tracker.as_hex(service_uuid)))
    elif len(service_uuid) == len(esp32_ble_tracker.bt_uuid128_format):
        cg.add(
            var.set_service_uuid128(
                esp32_ble_tracker.as_reversed_hex_array(service_uuid)
            )
        )

    char_uuid = config[CONF_CHARACTERISTIC_UUID]
    if len(char_uuid) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(var.set_char_uuid16(esp32_ble_tracker.as_hex(char_uuid)))
    elif len(char_uuid) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(var.set_char_uuid32(esp32_ble_tracker.as_hex(char_uuid)))
    elif len(char_uuid) == len(esp32_ble_tracker.bt_uuid128_format):
        cg.add(
            var.set_char_uuid128(esp32_ble_tracker.as_reversed_hex_array(char_uuid))
        )

    # Notification characteristic: the dedicated one if given, otherwise the write one
    # (combined NOTIFY+WRITE characteristic).
    notify_uuid = config.get(CONF_NOTIFY_CHARACTERISTIC_UUID, char_uuid)
    if len(notify_uuid) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(var.set_notify_char_uuid16(esp32_ble_tracker.as_hex(notify_uuid)))
    elif len(notify_uuid) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(var.set_notify_char_uuid32(esp32_ble_tracker.as_hex(notify_uuid)))
    elif len(notify_uuid) == len(esp32_ble_tracker.bt_uuid128_format):
        cg.add(
            var.set_notify_char_uuid128(
                esp32_ble_tracker.as_reversed_hex_array(notify_uuid)
            )
        )

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_spreading_factor(config[CONF_SPREADING_FACTOR]))
    cg.add(var.set_bandwidth(config[CONF_SIGNAL_BANDWIDTH]))
    cg.add(var.set_coding_rate(config[CONF_CODING_RATE]))
    cg.add(var.set_preamble_length(config[CONF_PREAMBLE_LENGTH]))
    cg.add(var.set_network_id(config[CONF_NETWORK_ID]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))
    cg.add(var.set_air_time(config[CONF_AIR_TIME]))
    cg.add(var.set_min_tx_interval_ms(config[CONF_MIN_TX_INTERVAL].total_milliseconds))
