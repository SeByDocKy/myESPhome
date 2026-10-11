"""APsystems (older generation, ZigBee based) micro-inverters: YC600 / QS1 / DS3.

100% native ESPHome component with two radio back-ends:

* ``radio: native`` (default): the IEEE 802.15.4 radio of an ESP32-C6 / ESP32-H2. The ZigBee NWK/APS framing is
  implemented in C++ and replaces the CC2530 used by the ESP32-ECU project.
* ``radio: znp``: a TI CC2530 running the Z-Stack ZNP firmware on an UART, exactly like the original project. Works with
  any ESP32 (and with the Arduino framework too).
"""

import re

import esphome.codegen as cg
from esphome import pins
from esphome.components import esp32, uart
from esphome.components.esp32.const import VARIANT_ESP32C6, VARIANT_ESP32H2
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_TYPE
from esphome.core import CORE

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = []
MULTI_CONF = False

apszb_ns = cg.esphome_ns.namespace("apszb")
APSZBComponent = apszb_ns.class_("APSZBComponent", cg.Component)
APSInverter = apszb_ns.class_("APSInverter")

CONF_APSZB_ID = "apszb_id"
CONF_INVERTER = "inverter"
CONF_INVERTERS = "inverters"
CONF_ECU_ID = "ecu_id"
CONF_SERIAL = "serial"
CONF_SHORT_ADDRESS = "short_address"
CONF_CALIBRATION = "calibration"
CONF_RATED_POWER = "rated_power"
CONF_TX_POWER = "tx_power"
CONF_POLL_INTERVAL = "poll_interval"
CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_LINK_STATUS_INTERVAL = "link_status_interval"
CONF_ROUTE_REQUEST_INTERVAL = "route_request_interval"
CONF_AUTO_PAIR = "auto_pair"
CONF_PROMISCUOUS = "promiscuous"
CONF_LOG_FRAMES = "log_frames"
CONF_NWK_PROTOCOL_VERSION = "nwk_protocol_version"
CONF_DISCOVER_ROUTE = "discover_route"
CONF_MAC_ACK = "mac_ack"
CONF_OFFLINE_AFTER = "offline_after"
CONF_RADIO = "radio"
CONF_RESET_PIN = "reset_pin"
CONF_UART_ID = "uart_id"

RADIO_NATIVE = "native"
RADIO_ZNP = "znp"

INVERTER_TYPES = {"yc600": 0, "qs1": 1, "ds3": 2}
DEFAULT_RATED_POWER = {"yc600": 600, "qs1": 1200, "ds3": 730}

_HEX12 = re.compile(r"^[0-9A-Fa-f]{12}$")


def hex12(value):
    value = cv.string_strict(value)
    if not _HEX12.match(value):
        raise cv.Invalid("expected exactly 12 hexadecimal digits, e.g. 'D8A3011B9780'")
    return value.upper()


def optional_period(value):
    """A time period that may be 0s (= feature disabled)."""
    return cv.positive_time_period_milliseconds(value)


INVERTER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(APSInverter),
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


def _validate_radio(config):
    if config[CONF_RADIO] == RADIO_NATIVE:
        for key in (CONF_UART_ID, CONF_RESET_PIN):
            if key in config:
                raise cv.Invalid(f"'{key}' is only used with 'radio: znp'", path=[key])
        # the IEEE 802.15.4 radio only exists on these chips
        esp32.only_on_variant(
            supported=[VARIANT_ESP32C6, VARIANT_ESP32H2], msg_prefix="apszb with 'radio: native' (IEEE 802.15.4)"
        )(config)
    else:
        if CONF_UART_ID not in config:
            raise cv.Invalid("'uart_id' is required with 'radio: znp'", path=[CONF_UART_ID])
    return config


def _final_validate(config):
    if config[CONF_RADIO] == RADIO_NATIVE:
        if CORE.using_arduino:
            raise cv.Invalid("apszb with 'radio: native' requires the esp-idf framework (raw IEEE 802.15.4 driver)")
    else:
        # the CC2530 ZNP firmware talks 115200 baud, 8N1
        uart.final_validate_device_schema("apszb", baud_rate=115200, require_tx=True, require_rx=True)(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(APSZBComponent),
            # native: ESP32-C6/H2 802.15.4 radio; znp: external CC2530 (Z-Stack ZNP firmware) on an UART
            cv.Optional(CONF_RADIO, default=RADIO_NATIVE): cv.one_of(RADIO_NATIVE, RADIO_ZNP, lower=True),
            cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            # RESET_N of the CC2530 (optional, driven low to reset the modem at start-up)
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            # the 12 hex digits printed on the ECU (or any 6 bytes you like for a DIY ECU)
            cv.Required(CONF_ECU_ID): hex12,
            cv.Optional(CONF_CHANNEL, default=16): cv.int_range(min=11, max=26),
            cv.Optional(CONF_TX_POWER, default=10): cv.int_range(min=-24, max=20),
            cv.Optional(CONF_POLL_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RESPONSE_TIMEOUT, default="2.5s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_OFFLINE_AFTER, default=3): cv.int_range(min=1, max=100),
            cv.Optional(CONF_AUTO_PAIR, default=True): cv.boolean,
            # ZigBee router/concentrator behaviour (0s disables)
            cv.Optional(CONF_LINK_STATUS_INTERVAL, default="15s"): optional_period,
            cv.Optional(CONF_ROUTE_REQUEST_INTERVAL, default="60s"): optional_period,
            # frame level knobs, useful to tune against a sniffer capture of a real ECU
            cv.Optional(CONF_NWK_PROTOCOL_VERSION, default=2): cv.int_range(min=1, max=2),
            cv.Optional(CONF_DISCOVER_ROUTE, default=False): cv.boolean,
            cv.Optional(CONF_MAC_ACK, default=True): cv.boolean,
            # diagnostics
            cv.Optional(CONF_PROMISCUOUS, default=False): cv.boolean,
            cv.Optional(CONF_LOG_FRAMES, default=False): cv.boolean,
            cv.Required(CONF_INVERTERS): cv.All(cv.ensure_list(INVERTER_SCHEMA), cv.Length(min=1, max=16)),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_radio,
)

FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_ecu_id(config[CONF_ECU_ID]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))
    cg.add(var.set_poll_interval(config[CONF_POLL_INTERVAL]))
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))
    cg.add(var.set_offline_after(config[CONF_OFFLINE_AFTER]))
    cg.add(var.set_auto_pair(config[CONF_AUTO_PAIR]))
    cg.add(var.set_link_status_interval(config[CONF_LINK_STATUS_INTERVAL]))
    cg.add(var.set_route_request_interval(config[CONF_ROUTE_REQUEST_INTERVAL]))
    cg.add(var.set_nwk_protocol_version(config[CONF_NWK_PROTOCOL_VERSION]))
    cg.add(var.set_discover_route(config[CONF_DISCOVER_ROUTE]))
    cg.add(var.set_mac_ack(config[CONF_MAC_ACK]))
    cg.add(var.set_promiscuous(config[CONF_PROMISCUOUS]))
    cg.add(var.set_log_frames(config[CONF_LOG_FRAMES]))

    for conf in config[CONF_INVERTERS]:
        inv = cg.new_Pvariable(conf[CONF_ID])
        cg.add(inv.set_serial(conf[CONF_SERIAL]))
        cg.add(inv.set_type(conf[CONF_TYPE]))
        cg.add(inv.set_calibration(conf[CONF_CALIBRATION]))
        cg.add(inv.set_rated_power(conf.get(CONF_RATED_POWER, DEFAULT_RATED_POWER[conf[CONF_TYPE]])))
        if CONF_SHORT_ADDRESS in conf:
            cg.add(inv.set_short_address(conf[CONF_SHORT_ADDRESS]))
        cg.add(var.add_inverter(inv))

    if config[CONF_RADIO] == RADIO_ZNP:
        cg.add_define("USE_APSZB_ZNP")
        uart_var = await cg.get_variable(config[CONF_UART_ID])
        cg.add(var.set_uart(uart_var))
        if CONF_RESET_PIN in config:
            pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
            cg.add(var.set_reset_pin(pin))
    else:
        cg.add_define("USE_APSZB_NATIVE")
        # The raw 802.15.4 driver is excluded from ESPHome's default ESP-IDF build
        esp32.add_idf_sdkconfig_option("CONFIG_IEEE802154_ENABLED", True)
        for component in ("ieee802154", "esp_hal_ieee802154", "esp_phy", "esp_coex"):
            esp32.include_builtin_idf_component(component)
