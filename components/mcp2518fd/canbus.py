import esphome.codegen as cg
from esphome.components import canbus, spi
from esphome.components.canbus import CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID

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


def _validate_can_speed(value):
    """Accept both legacy enum style ('500kbps'/'500KBPS') and
    frequency style ('500kHz') used in ESPHome 2026.6+."""
    if isinstance(value, str):
        # 1. Direct enum match (e.g. '500KBPS', '500kbps' via upper())
        upper = value.upper()
        if upper in canbus.CAN_SPEEDS:
            return upper
        # 2. Frequency notation (e.g. '500kHz' -> 500000 Hz)
        try:
            hz = cv.frequency(value)
            bps = int(hz)
            for key in canbus.CAN_SPEEDS:
                if canbus.get_rate(key) == bps:
                    return key
        except (cv.Invalid, ValueError):
            pass
        # 3. Explicit bps notation (e.g. '500kbps' -> 500000 bps)
        try:
            bps_val = cv.bps(value)
            bps = int(bps_val)
            for key in canbus.CAN_SPEEDS:
                if canbus.get_rate(key) == bps:
                    return key
        except (cv.Invalid, ValueError):
            pass
    raise cv.Invalid(
        f"Invalid CAN speed '{value}'. "
        "Use e.g. '500kbps', '500KBPS', or '500kHz'."
    )


CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(mcp2518fd),
        # Override bit_rate to accept both old ('500kbps') and new ('500kHz') formats
        cv.Optional("bit_rate", default="125KBPS"): _validate_can_speed,
        cv.Optional(CONF_MCP_CLOCK, default="40MHz"): cv.enum(CAN_CLOCK),
        cv.Optional(CONF_MCP_MODE,  default="NORMAL"): cv.enum(CAN_MODE),
        cv.Optional(CONF_CANFD_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_DATA_RATE, default="500KBPS"): _validate_can_speed,
    }
).extend(spi.spi_device_schema(True))


async def to_code(config):
    rhs = mcp2518fd.new()
    var = cg.Pvariable(config[CONF_ID], rhs)
    await canbus.register_canbus(var, config)

    cg.add(var.set_mcp_clock(CAN_CLOCK[config[CONF_MCP_CLOCK]]))
    cg.add(var.set_mcp_mode(CAN_MODE[config[CONF_MCP_MODE]]))
    cg.add(var.set_canfd_enabled(config[CONF_CANFD_ENABLED]))
    cg.add(var.set_data_rate(canbus.CAN_SPEEDS[config[CONF_DATA_RATE]]))

    await spi.register_spi_device(var, config)
