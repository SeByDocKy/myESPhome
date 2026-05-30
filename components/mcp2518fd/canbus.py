import esphome.codegen as cg
from esphome.components import canbus, spi
from esphome.components.canbus import CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID
import voluptuous as vol

CODEOWNERS = ["@sebyd"]
DEPENDENCIES = ["spi"]

CONF_MCP_CLOCK     = "mcp_clock"
CONF_MCP_MODE      = "mcp_mode"
CONF_CANFD_ENABLED = "canfd_enabled"
CONF_DATA_RATE     = "data_rate"

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


def _normalize_speed(value):
    """Normalize a CAN speed string to the uppercase key used in CAN_SPEEDS.

    Accepts all three formats used across ESPHome versions:
      - '500kbps' / '500KBPS'   (legacy enum style, all versions)
      - '500kHz'  / '1MHz'      (frequency style, ESPHome >= 2026.6-dev)
      - '500000bps'              (explicit bps)
    """
    if not isinstance(value, str):
        raise cv.Invalid(f"Expected a string for CAN speed, got {type(value)}")
    upper = value.upper()
    # 1. Direct match in CAN_SPEEDS dict  (e.g. '500KBPS')
    if upper in canbus.CAN_SPEEDS:
        return upper
    # 2. bps notation  (e.g. '500kbps' -> 500000 bps)
    try:
        bps = int(cv.bps(value))
        for key in canbus.CAN_SPEEDS:
            if canbus.get_rate(key) == bps:
                return key
    except (cv.Invalid, ValueError):
        pass
    # 3. Frequency notation  (e.g. '500kHz' -> 500000 Hz == 500 kbps)
    try:
        hz = int(cv.frequency(value))
        for key in canbus.CAN_SPEEDS:
            if canbus.get_rate(key) == hz:
                return key
    except (cv.Invalid, ValueError):
        pass
    raise cv.Invalid(
        f"Invalid CAN speed '{value}'. "
        "Use e.g. '500kbps', '500KBPS', or '500kHz'."
    )


def _normalize_bit_rate(config):
    """Pre-process config dict: normalize bit_rate BEFORE CANBUS_SCHEMA validates it.

    This ensures compatibility with both:
      - ESPHome <= 2026.5  which uses cv.enum(CAN_SPEEDS) for bit_rate
      - ESPHome >= 2026.6  which uses cv.frequency for bit_rate
    """
    if "bit_rate" in config and isinstance(config["bit_rate"], str):
        try:
            normalized = _normalize_speed(config["bit_rate"])
            config = dict(config)
            config["bit_rate"] = normalized
        except cv.Invalid:
            pass  # leave as-is; CANBUS_SCHEMA will raise a clear error
    return config


CONFIG_SCHEMA = cv.Schema(
    vol.All(
        _normalize_bit_rate,
        canbus.CANBUS_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(mcp2518fd),
                cv.Optional(CONF_MCP_CLOCK, default="40MHz"): cv.enum(CAN_CLOCK),
                cv.Optional(CONF_MCP_MODE,  default="NORMAL"): cv.enum(CAN_MODE),
                cv.Optional(CONF_CANFD_ENABLED, default=False): cv.boolean,
                cv.Optional(CONF_DATA_RATE, default="500KBPS"): _normalize_speed,
            }
        ).extend(spi.spi_device_schema(True)),
    )
)


async def to_code(config):
    rhs = mcp2518fd.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    await canbus.register_canbus(var, config)

    cg.add(var.set_mcp_clock(CAN_CLOCK[config[CONF_MCP_CLOCK]]))
    cg.add(var.set_mcp_mode(CAN_MODE[config[CONF_MCP_MODE]]))
    cg.add(var.set_canfd_enabled(config[CONF_CANFD_ENABLED]))
    cg.add(var.set_data_rate(canbus.CAN_SPEEDS[config[CONF_DATA_RATE]]))

    await spi.register_spi_device(var, config)
