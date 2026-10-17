"""WCH CH9438: SPI to 8 UART (full duplex) + 8 GPIO, native ESPHome component."""

from esphome import automation, pins
import logging

import esphome.codegen as cg
from esphome.components import spi, uart
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_CHANNEL,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArguments
from esphome.types import ConfigType, TemplateArgsType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["uart"]
MULTI_CONF = True

CONF_CH9438 = "ch9438"
CONF_CH9438_ID = "ch9438_id"
CONF_CRYSTAL = "crystal"
CONF_IRQ_PIN = "irq_pin"
CONF_RESET_PIN = "reset_pin"
CONF_UART = "uart"
CONF_RX_BUFFER_SIZE = "rx_buffer_size"
CONF_FLOW_CONTROL = "flow_control"
CONF_RS485 = "rs485"
CONF_ENABLED = "enabled"
CONF_INVERT = "invert"
CONF_TEST_MODE = "test_mode"

NUM_CHANNELS = 8
NUM_GPIO = 8
UART_REF_CLOCK = 192000000  # internal clock: baud = clock / divisor
PLL_MULTIPLIER = 12  # an external crystal is multiplied by 12 (16 MHz -> 192 MHz)
CRYSTAL_DOCUMENTED = 16000000  # the only crystal frequency documented by the vendor
DIVISOR_MIN = 16
DIVISOR_MAX = 0xFFFF
MAX_DATA_RATE_HZ = 25000000

FLOW_NONE = "none"
FLOW_CTS = "cts"
FLOW_RTS_CTS = "rts_cts"
FLOW_CONTROL_OPTIONS = [FLOW_NONE, FLOW_CTS, FLOW_RTS_CTS]

# The 8 multiplexed physical pins are named after their GPIO number. Each of them can carry only ONE of the
# functions listed below (CH9438 datasheet V1.1, pin description):
#   GPIO0: CTS1 / TNOW0            GPIO4: RTS3 / TNOW4
#   GPIO1: RTS1 / TNOW1 / INT#     GPIO5: CTS3 / TNOW5
#   GPIO2: RTS2 / TNOW2            GPIO6: RTS4 / TNOW6
#   GPIO3: CTS2 / TNOW3            GPIO7: CTS4 / TNOW7 / XO (crystal)
CTS_PIN = {1: 0, 2: 3, 3: 5, 4: 7}  # UART number -> physical pin (GPIO number)
RTS_PIN = {1: 1, 2: 2, 3: 4, 4: 6}
INT_PIN = 1
XTAL_PIN = 7

ch9438_ns = cg.esphome_ns.namespace("ch9438")
CH9438Component = ch9438_ns.class_("CH9438Component", cg.Component, spi.SPIDevice)
CH9438Channel = ch9438_ns.class_("CH9438Channel", uart.UARTComponent)
CH9438GPIOPin = ch9438_ns.class_("CH9438GPIOPin", cg.GPIOPin)
ChangeBaudRateAction = ch9438_ns.class_("ChangeBaudRateAction", automation.Action)


def validate_rs485(value):
    """`rs485: true` or `rs485: {enabled: true, invert: false}`."""
    if isinstance(value, dict):
        return RS485_SCHEMA(value)
    return {CONF_ENABLED: cv.boolean(value), CONF_INVERT: False}


RS485_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_INVERT, default=False): cv.boolean,
    }
)


def uart_clock(crystal_hz: int) -> int:
    """UART reference clock: 192 MHz (internal) or 12 x the crystal."""
    return crystal_hz * PLL_MULTIPLIER if crystal_hz else UART_REF_CLOCK


def baud_limits(crystal_hz: int) -> tuple[int, int]:
    """Lowest / highest baud rate for the 16 bit divisor (12.4 fixed point, smallest value 16)."""
    clock = uart_clock(crystal_hz)
    return -(-clock // DIVISOR_MAX), clock // DIVISOR_MIN


def validate_crystal(value):
    """`crystal: 16MHz` (or a number of Hz). `false` = internal clock, `true` = 16 MHz (older syntax)."""
    if isinstance(value, bool):
        return CRYSTAL_DOCUMENTED if value else 0
    if isinstance(value, str) and value.strip().lower() in ("true", "yes", "on"):
        return CRYSTAL_DOCUMENTED
    if isinstance(value, str) and value.strip().lower() in ("false", "no", "off"):
        return 0
    hz = int(round(cv.frequency(value)))
    if hz == 0:
        return 0
    if not 1000000 <= hz <= 32000000:
        raise cv.Invalid("crystal: the frequency must be between 1 MHz and 32 MHz")
    return hz


def validate_baud_rate(value):
    # the exact range depends on the clock, it is checked in validate_baud_rates()
    return cv.int_range(min=1, max=12000000)(value)


UART_CHANNEL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(CH9438Channel),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=NUM_CHANNELS - 1),
        cv.Required(CONF_BAUD_RATE): validate_baud_rate,
        cv.Optional(CONF_DATA_BITS, default=8): cv.one_of(8, int=True),
        cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
        cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
            uart.UART_PARITY_OPTIONS, upper=True
        ),
        cv.Optional(CONF_RX_BUFFER_SIZE, default=256): cv.int_range(
            min=64, max=4096
        ),
        cv.Optional(CONF_FLOW_CONTROL, default=FLOW_NONE): cv.one_of(
            *FLOW_CONTROL_OPTIONS, lower=True
        ),
        cv.Optional(CONF_RS485): validate_rs485,
    }
)


def pin_claims(config: ConfigType) -> dict[int, list[str]]:
    """Physical multiplexed pins (named after their GPIO number) used by the configuration."""
    claims: dict[int, list[str]] = {}

    def claim(pin: int, what: str) -> None:
        claims.setdefault(pin, []).append(what)

    if CONF_IRQ_PIN in config:
        claim(INT_PIN, "INT# (irq_pin)")
    if config[CONF_CRYSTAL]:
        claim(XTAL_PIN, "the crystal output XO (crystal)")
    for conf in config[CONF_UART]:
        channel = conf[CONF_CHANNEL]
        rs485 = conf.get(CONF_RS485)
        if rs485 and rs485[CONF_ENABLED]:
            claim(channel, f"TNOW{channel} (rs485 on UART{channel})")
        flow = conf[CONF_FLOW_CONTROL]
        if flow in (FLOW_CTS, FLOW_RTS_CTS):
            claim(CTS_PIN[channel], f"CTS{channel} (flow_control on UART{channel})")
        if flow == FLOW_RTS_CTS:
            claim(RTS_PIN[channel], f"RTS{channel} (flow_control on UART{channel})")
    return claims


def validate_channels(config: ConfigType) -> ConfigType:
    seen = set()
    for conf in config[CONF_UART]:
        channel = conf[CONF_CHANNEL]
        if channel in seen:
            raise cv.Invalid(f"Duplicate UART channel {channel}")
        seen.add(channel)
        flow = conf[CONF_FLOW_CONTROL]
        if flow != FLOW_NONE and channel not in CTS_PIN:
            raise cv.Invalid(
                f"UART{channel}: hardware flow control is only available on UART1 to UART4"
            )
    return config


def validate_pin_conflicts(config: ConfigType) -> ConfigType:
    for pin, users in pin_claims(config).items():
        if len(users) > 1:
            raise cv.Invalid(
                f"The chip pin shared with GPIO{pin} cannot be used by {' and '.join(users)} at the same time"
            )
    return config


def validate_baud_rates(config: ConfigType) -> ConfigType:
    crystal = config[CONF_CRYSTAL]
    baud_min, baud_max = baud_limits(crystal)
    clock_mhz = uart_clock(crystal) / 1e6
    for conf in config[CONF_UART]:
        baud = conf[CONF_BAUD_RATE]
        if not baud_min <= baud <= baud_max:
            raise cv.Invalid(
                f"UART{conf[CONF_CHANNEL]}: baud rate {baud} is not possible with the UART clock of {clock_mhz:g} MHz "
                f"(allowed range {baud_min} to {baud_max}). "
                "A lower crystal frequency gives lower baud rates."
            )
    if crystal and crystal != CRYSTAL_DOCUMENTED:
        _LOGGER.warning(
            "ch9438: crystal %g MHz - only 16 MHz is documented by the vendor, other values are untested "
            "(the PLL and the internal delays of the chip may not behave as expected)",
            crystal / 1e6,
        )
    return config


def validate_data_rate(config: ConfigType) -> ConfigType:
    rate = config.get(spi.CONF_DATA_RATE)
    if rate is not None and int(rate) > MAX_DATA_RATE_HZ:
        raise cv.Invalid(
            f"{spi.CONF_DATA_RATE}: the CH9438 supports at most 25 MHz on its SPI interface"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CH9438Component),
            cv.Optional(CONF_CRYSTAL, default=0): validate_crystal,
            cv.Optional(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
            # optional RST# output: the chip is reset at start-up (low 100 ms, high 100 ms) as the vendor code does
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            # 0: normal operation, 1: loopback of a short message, 2: loopback of a 1024 byte pattern with throughput
            # measurement. Modes 1 and 2 also log the bus/CPU statistics every 10 s. TX and RX of the tested UARTs
            # must be wired together.
            cv.Optional(CONF_TEST_MODE, default=0): cv.int_range(min=0, max=2),
            cv.Required(CONF_UART): cv.ensure_list(UART_CHANNEL_SCHEMA),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        # SPI mode 0 (CPOL=0, CPHA=0) as required by the vendor evaluation board manual, MSB first
        spi.spi_device_schema(
            cs_pin_required=True, default_data_rate="4MHz", default_mode="MODE0"
        )
    ),
    validate_channels,
    validate_pin_conflicts,
    validate_baud_rates,
    validate_data_rate,
)


# Schema fragment shared by the sub-platforms (text_sensor)
CH9438_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CH9438_ID): cv.use_id(CH9438Component),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_name(str(config[CONF_ID])))
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_crystal(config[CONF_CRYSTAL]))
    if config[CONF_TEST_MODE]:
        cg.add_define("CH9438_TEST_COMPONENT")
        cg.add(var.set_test_mode(config[CONF_TEST_MODE]))
    if irq_conf := config.get(CONF_IRQ_PIN):
        irq_pin = await cg.gpio_pin_expression(irq_conf)
        cg.add(var.set_irq_pin(irq_pin))

    if reset_conf := config.get(CONF_RESET_PIN):
        reset_pin = await cg.gpio_pin_expression(reset_conf)
        cg.add(var.set_reset_pin(reset_pin))

    tnow_mask = tnow_invert = cts_mask = rts_mask = 0
    for conf in config[CONF_UART]:
        channel = conf[CONF_CHANNEL]
        if (rs485 := conf.get(CONF_RS485)) and rs485[CONF_ENABLED]:
            tnow_mask |= 1 << channel
            if rs485[CONF_INVERT]:
                tnow_invert |= 1 << channel
        flow = conf[CONF_FLOW_CONTROL]
        if flow in (FLOW_CTS, FLOW_RTS_CTS):
            cts_mask |= 1 << channel
        if flow == FLOW_RTS_CTS:
            rts_mask |= 1 << channel
    cg.add(var.set_tnow_mask(tnow_mask, tnow_invert))
    cg.add(var.set_cts_mask(cts_mask))
    cg.add(var.set_rts_mask(rts_mask))

    for conf in config[CONF_UART]:
        chan = cg.new_Pvariable(conf[CONF_ID])
        cg.add(chan.set_channel_name(str(conf[CONF_ID])))
        cg.add(chan.set_parent(var))
        cg.add(chan.set_channel(conf[CONF_CHANNEL]))
        cg.add(chan.set_baud_rate(conf[CONF_BAUD_RATE]))
        cg.add(chan.set_data_bits(conf[CONF_DATA_BITS]))
        cg.add(chan.set_stop_bits(conf[CONF_STOP_BITS]))
        cg.add(chan.set_parity(conf[CONF_PARITY]))
        cg.add(chan.set_rx_buffer_size(conf[CONF_RX_BUFFER_SIZE]))
        cg.add(chan.set_flow_control(conf[CONF_FLOW_CONTROL] != FLOW_NONE))


# ---------------------------------------------------------------------------------------------------------------------
# Action:  ch9438.change_baud_rate: {id: <uart id>, baud_rate: 9600}
# ---------------------------------------------------------------------------------------------------------------------
CHANGE_BAUD_RATE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(CH9438Channel),
        cv.Required(CONF_BAUD_RATE): cv.templatable(
            cv.int_range(min=1, max=12000000)  # the exact limits depend on the crystal, checked at run time
        ),
    }
)


@automation.register_action(
    "ch9438.change_baud_rate",
    ChangeBaudRateAction,
    CHANGE_BAUD_RATE_SCHEMA,
    synchronous=True,
)
async def change_baud_rate_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_BAUD_RATE], args, cg.uint32)
    cg.add(var.set_baud_rate(template_))
    return var


# ---------------------------------------------------------------------------------------------------------------------
# GPIO pins:  pin: {ch9438: <hub id>, number: 0..7, mode: ...}
# ---------------------------------------------------------------------------------------------------------------------
def validate_pin_mode(value: ConfigType) -> ConfigType:
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_PULLUP] and not value[CONF_INPUT]:
        raise cv.Invalid("Pullup only available with input")
    if value[CONF_PULLDOWN] and not value[CONF_INPUT]:
        raise cv.Invalid("Pulldown only available with input")
    if value[CONF_PULLUP] and value[CONF_PULLDOWN]:
        raise cv.Invalid("Can only have one of pullup or pulldown")
    if value[CONF_OPEN_DRAIN] and not value[CONF_OUTPUT]:
        raise cv.Invalid("Open drain available only with output")
    return value


CH9438_PIN_SCHEMA = pins.gpio_base_schema(
    CH9438GPIOPin,
    cv.int_range(min=0, max=NUM_GPIO - 1),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_PULLUP, CONF_PULLDOWN, CONF_OPEN_DRAIN],
    mode_validator=validate_pin_mode,
).extend(
    {
        cv.Required(CONF_CH9438): cv.use_id(CH9438Component),
    }
)


def ch9438_pin_final_validate(pin_config: ConfigType, parent_config: ConfigType) -> None:
    number = pin_config[CONF_NUMBER]
    users = pin_claims(parent_config).get(number)
    if users:
        raise cv.Invalid(
            f"GPIO{number} is not available: its chip pin is used by {' and '.join(users)}"
        )


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_CH9438, CH9438_PIN_SCHEMA, ch9438_pin_final_validate
)
async def ch9438_pin_to_code(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_CH9438])
    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
