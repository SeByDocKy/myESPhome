from esphome import automation
import esphome.codegen as cg
import esphome.pins as pins
from esphome.components import canbus, spi
from esphome.components.canbus import (
    CanbusComponent, CanbusTrigger, CAN_SPEEDS,
    CONF_CAN_ID, CONF_BIT_RATE, CONF_ON_FRAME, CONF_USE_EXTENDED_ID,
    CONF_CAN_ID_MASK, CONF_REMOTE_TRANSMISSION_REQUEST, CONF_TRIGGER_ID,
    validate_id, setup_canbus_core_,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NUMBER, CONF_INVERTED

CODEOWNERS = ["@sebyd"]
DEPENDENCIES = ["spi"]

CONF_CAN_CLOCK      = "can_clock"
CONF_MCP_MODE       = "mode"
CONF_CANFD_ENABLED  = "canfd_enabled"
CONF_CAN_DATA_RATE  = "can_data_rate"
CONF_INT_PIN        = "int_pin"
CONF_INT0_PIN       = "int0_pin"
CONF_INT1_PIN       = "int1_pin"

mcp2518fd_ns = cg.esphome_ns.namespace("mcp2518fd")
mcp2518fd = mcp2518fd_ns.class_("MCP2518FD", CanbusComponent, spi.SPIDevice)

CanClock = mcp2518fd_ns.enum("CanClock")
CanMode  = mcp2518fd_ns.enum("CanMode")

CAN_CLOCK = {
    "4MHz":  CanClock.MCP_4MHZ,
    "10MHz": CanClock.MCP_10MHZ,
    "20MHz": CanClock.MCP_20MHZ,
    "40MHz": CanClock.MCP_40MHZ,
}

CAN_MODE = {
    "NORMAL":        CanMode.CAN_MODE_NORMAL,
    "LISTEN_ONLY":   CanMode.CAN_MODE_LISTEN_ONLY,
    "LOOPBACK_INT":  CanMode.CAN_MODE_LOOPBACK_INT,
    "LOOPBACK_EXT":  CanMode.CAN_MODE_LOOPBACK_EXT,
    "SLEEP":         CanMode.CAN_MODE_SLEEP,
    "CLASSIC":       CanMode.CAN_MODE_CLASSIC,
    "RESTRICTED":    CanMode.CAN_MODE_RESTRICTED,
}


def _can_speed(value):
    """Accept '500kbps', '500KBPS', or '500kHz'."""
    if isinstance(value, str):
        if value.upper() in CAN_SPEEDS:
            return value.upper()
        try:
            bps = int(cv.bps(value))
            for k in CAN_SPEEDS:
                if canbus.get_rate(k) == bps:
                    return k
        except (cv.Invalid, ValueError):
            pass
        try:
            hz = int(cv.frequency(value))
            for k in CAN_SPEEDS:
                if canbus.get_rate(k) == hz:
                    return k
        except (cv.Invalid, ValueError):
            pass
    raise cv.Invalid(
        f"Invalid CAN speed '{value}'. Use e.g. '500kbps', '500KBPS', or '500kHz'."
    )


def _int_pin_schema(value):
    """Validate an interrupt pin.

    The MCP2518FD INT/INT0/INT1 pins are ALWAYS active-low (open-drain).
    The user just specifies the GPIO number — inverted is forced to True
    internally so that digital_read() returns True when the interrupt fires.

    Accepts both shorthand (just a pin number) and dict form:
        int1_pin: GPIO4
        int1_pin:
          number: GPIO4
          # inverted is always forced to True — no need to declare it
    """
    # Normalise shorthand to dict
    if not isinstance(value, dict):
        value = {CONF_NUMBER: value}
    # Force inverted=True regardless of what the user wrote
    value = dict(value)
    value[CONF_INVERTED] = True
    return pins.gpio_input_pin_schema(value)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(mcp2518fd),
            cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=0x1FFFFFFF),
            cv.Optional(CONF_BIT_RATE, default="125KBPS"): _can_speed,
            cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
            cv.Optional(CONF_ON_FRAME): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CanbusTrigger),
                    cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=0x1FFFFFFF),
                    cv.Optional(CONF_CAN_ID_MASK, default=0x1FFFFFFF):
                        cv.int_range(min=0, max=0x1FFFFFFF),
                    cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
                    cv.Optional(CONF_REMOTE_TRANSMISSION_REQUEST): cv.boolean,
                },
                validate_id,
            ),
            cv.Optional(CONF_CAN_CLOCK,     default="40MHz"):   cv.enum(CAN_CLOCK),
            cv.Optional(CONF_MCP_MODE,      default="NORMAL"):  cv.enum(CAN_MODE),
            cv.Optional(CONF_CANFD_ENABLED, default=False):     cv.boolean,
            cv.Optional(CONF_CAN_DATA_RATE, default="500KBPS"): _can_speed,
            # Interrupt pins — inverted forced internally (always active-low)
            cv.Optional(CONF_INT_PIN):  _int_pin_schema,
            cv.Optional(CONF_INT0_PIN): _int_pin_schema,
            cv.Optional(CONF_INT1_PIN): _int_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)

CONFIG_SCHEMA.add_extra(validate_id)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_canbus_core_(var, config)

    cg.add(var.set_can_clock(CAN_CLOCK[config[CONF_CAN_CLOCK]]))
    cg.add(var.set_mcp_mode(CAN_MODE[config[CONF_MCP_MODE]]))
    cg.add(var.set_canfd_enabled(config[CONF_CANFD_ENABLED]))
    cg.add(var.set_can_data_rate(CAN_SPEEDS[config[CONF_CAN_DATA_RATE]]))

    if CONF_INT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INT_PIN])
        cg.add(var.set_int_pin(pin))

    if CONF_INT0_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INT0_PIN])
        cg.add(var.set_int0_pin(pin))

    if CONF_INT1_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_INT1_PIN])
        cg.add(var.set_int1_pin(pin))

    await spi.register_spi_device(var, config)
