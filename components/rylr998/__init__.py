import logging

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    CONF_TRIGGER_ID,
    CONF_UART_ID,
)

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@SeByDocKy,@claude"]
AUTO_LOAD = ["packet_transport", "sensor", "number"]
MULTI_CONF = True

CONF_RYLR998_ID = "rylr998_id"
CONF_ADDRESS = "address"
CONF_SPREADING_FACTOR = "spreading_factor"
CONF_SIGNAL_BANDWIDTH = "signal_bandwidth"
CONF_CODING_RATE = "coding_rate"
CONF_PREAMBLE_LENGTH = "preamble_length"
CONF_NETWORK_ID = "network_id"
CONF_TX_POWER = "tx_power"
CONF_ON_PACKET = "on_packet"
CONF_DESTINATION = "destination"
CONF_DATA = "data"
CONF_AIR_TIME = "air_time"
CONF_VARIANT = "variant"

# [VARIANT] Le RYLR998 et le RYLR999 partagent le même jeu de commandes AT sur
#           le port LoRa (vérifié contre la doc REYAX des deux modules).
#           Seule différence fonctionnelle connue : la puissance RF max du PA.
VARIANT_RYLR998 = "rylr998"
VARIANT_RYLR999 = "rylr999"
VARIANTS = [VARIANT_RYLR998, VARIANT_RYLR999]

# Spreading factors documented by REYAX for each bandwidth (AT+PARAMETER)
SF_RANGE_BY_BANDWIDTH = {
    125000: (7, 9),
    250000: (7, 10),
    500000: (7, 11),
}

# Longest line of the module is a ~270 byte +RCV; at 115200 baud the default 256 byte UART buffer
# fills in 22 ms, so a burst (+RCV followed by other lines) can overflow it if loop() is delayed.
RECOMMENDED_RX_BUFFER = 512

# AT+CRFOP : 0-22dBm sur RYLR998 (SX1262), 0-30dBm sur RYLR999
TX_POWER_MAX_BY_VARIANT = {
    VARIANT_RYLR998: 22,
    VARIANT_RYLR999: 30,
}

rylr998_ns = cg.esphome_ns.namespace("rylr998")

RYLR998Component = rylr998_ns.class_(
    "RYLR998Component", cg.Component, uart.UARTDevice
)

RYLRVariant = rylr998_ns.enum("RYLRVariant", is_class=True)
VARIANT_ENUM = {
    VARIANT_RYLR998: RYLRVariant.RYLR998,
    VARIANT_RYLR999: RYLRVariant.RYLR999,
}

RYLR998PacketTrigger = rylr998_ns.class_(
    "RYLR998PacketTrigger",
    automation.Trigger.template(
        cg.std_vector.template(cg.uint8), cg.float_, cg.float_
    ),
)

RYLR998SendPacketAction = rylr998_ns.class_(
    "RYLR998SendPacketAction", automation.Action
)

RYLR998ResetAction = rylr998_ns.class_(
    "RYLR998ResetAction", automation.Action, cg.Parented.template(RYLR998Component)
)


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


def _validate_variant_constraints(config):
    variant = config[CONF_VARIANT]
    max_power = TX_POWER_MAX_BY_VARIANT[variant]
    if config[CONF_TX_POWER] > max_power:
        raise cv.Invalid(
            f"tx_power maximum for variant '{variant}' is {max_power} dBm "
            f"(got {config[CONF_TX_POWER]} dBm)"
        )

    # REYAX AT guide (AT+NETWORKID, preamble note): the preamble length can only be changed
    # (4-24) when NETWORKID is 18; for any other network id the module refuses it (+ERR=18).
    # Documented for the RYLR998 (p.5) as well as for the RYLR999.
    preamble = config[CONF_PREAMBLE_LENGTH]
    network_id = config[CONF_NETWORK_ID]
    if network_id != 18 and preamble != 12:
        raise cv.Invalid(
            "preamble_length can only be changed (4-24) when network_id is 18. "
            "For any other network_id it must stay at 12 (the module answers +ERR=18 otherwise)."
        )

    # Not an error (the module may accept it), but outside the documented combinations.
    bandwidth = config[CONF_SIGNAL_BANDWIDTH]
    sf = config[CONF_SPREADING_FACTOR]
    low, high = SF_RANGE_BY_BANDWIDTH[int(bandwidth)]
    if not low <= sf <= high:
        _LOGGER.warning(
            "rylr998: spreading_factor %s is outside the range documented by REYAX for a "
            "%s kHz bandwidth (SF%s-SF%s); the module may refuse it",
            sf,
            int(bandwidth) // 1000,
            low,
            high,
        )
    return config


def _final_validate(config):
    full = fv.full_config.get()
    uarts = full.get("uart") or []
    if isinstance(uarts, dict):
        uarts = [uarts]
    for u in uarts:
        if u.get(CONF_ID) == config[CONF_UART_ID]:
            size = u.get("rx_buffer_size", 256)
            if size < RECOMMENDED_RX_BUFFER:
                _LOGGER.warning(
                    "rylr998: the uart '%s' has rx_buffer_size: %s; at least %s is recommended "
                    "(a +RCV line can reach ~270 bytes and a full 256-byte buffer is filled in "
                    "22 ms at 115200 baud)",
                    config[CONF_UART_ID],
                    size,
                    RECOMMENDED_RX_BUFFER,
                )
            break
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RYLR998Component),
            cv.Optional(CONF_VARIANT, default=VARIANT_RYLR998): cv.one_of(
                *VARIANTS, lower=True
            ),
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=65535),
            cv.Optional(CONF_FREQUENCY, default=915000000): cv.frequency,
            cv.Optional(CONF_SPREADING_FACTOR, default=9): validate_spreading_factor,
            cv.Optional(CONF_SIGNAL_BANDWIDTH, default=125000): validate_bandwidth,
            cv.Optional(CONF_CODING_RATE, default=1): validate_coding_rate,
            cv.Optional(CONF_PREAMBLE_LENGTH, default=12): cv.int_range(min=4, max=24),
            cv.Optional(CONF_NETWORK_ID, default=18): validate_network_id,
            # [VARIANT] max porté à 30 ici (borne large) ; la borne réelle par
            #           variante (22 ou 30) est vérifiée dans _validate_variant_constraints
            #           ci-dessous, et re-vérifiée au runtime côté C++ (clamp défensif).
            cv.Optional(CONF_TX_POWER, default=22): cv.int_range(min=0, max=30),
            cv.Optional(CONF_AIR_TIME, default=True): cv.boolean,
            cv.Optional(CONF_ON_PACKET): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RYLR998PacketTrigger),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate_variant_constraints,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
    cg.add(var.set_spreading_factor(config[CONF_SPREADING_FACTOR]))
    cg.add(var.set_bandwidth(int(config[CONF_SIGNAL_BANDWIDTH])))
    cg.add(var.set_coding_rate(config[CONF_CODING_RATE]))
    cg.add(var.set_preamble_length(config[CONF_PREAMBLE_LENGTH]))
    cg.add(var.set_network_id(config[CONF_NETWORK_ID]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))
    cg.add(var.set_air_time(config[CONF_AIR_TIME]))
    cg.add(var.set_variant(VARIANT_ENUM[config[CONF_VARIANT]]))

    for conf in config.get(CONF_ON_PACKET, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.add_packet_trigger(trigger))
        await automation.build_automation(
            trigger,
            [
                (cg.std_vector.template(cg.uint8), "data"),
                (cg.float_, "rssi"),
                (cg.float_, "snr"),
            ],
            conf,
        )


@automation.register_action(
    "rylr998.send_packet",
    RYLR998SendPacketAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(RYLR998Component),
            cv.Required(CONF_DESTINATION): cv.templatable(cv.int_range(min=0, max=65535)),
            cv.Required(CONF_DATA): cv.templatable(cv.ensure_list(cv.uint8_t)),
        }
    ),
    synchronous=True,
)
async def rylr998_send_packet_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_DESTINATION], args, cg.uint16)
    cg.add(var.set_destination(template_))
    data = config[CONF_DATA]
    if isinstance(data, cv.Lambda):
        template_ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(template_))
    else:
        cg.add(var.set_data_static(data))
    return var


@automation.register_action(
    "rylr998.reset",
    RYLR998ResetAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(RYLR998Component),
        }
    ),
    synchronous=True,
)
async def rylr998_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
