"""ESPHome canbus platform for the Microchip MCP2518FD CAN FD controller.

This file registers the ``mcp2518fd`` platform under the ``canbus`` component.
It mirrors the structure of esphome/components/mcp2515/canbus.py.

YAML usage example:

.. code-block:: yaml

    canbus:
      - platform: mcp2518fd
        id: my_can_bus
        cs_pin: GPIO5
        can_id: 4
        bit_rate: 500kbps
        mcp_clock: 40MHz
        mcp_mode: NORMAL
        canfd_enabled: false
        data_rate: 500kbps
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus, spi
from esphome.const import CONF_ID

# ------------------------------------------------------------------
# Namespace / class binding
# ------------------------------------------------------------------
mcp2518fd_ns = cg.esphome_ns.namespace("mcp2518fd")

# The C++ class inherits from canbus::Canbus — Python side uses CanbusComponent
MCP2518FD = mcp2518fd_ns.class_("MCP2518FD", canbus.CanbusComponent, spi.SPIDevice)

# ------------------------------------------------------------------
# Enum bindings  (mirror mcp2515 naming where possible)
# ------------------------------------------------------------------
CanClockEnum = mcp2518fd_ns.enum("CanClock")
CAN_CLOCK = {
    "4MHz":  CanClockEnum.MCP_4MHZ,
    "10MHz": CanClockEnum.MCP_10MHZ,
    "20MHz": CanClockEnum.MCP_20MHZ,
    "40MHz": CanClockEnum.MCP_40MHZ,
}

CanModeEnum = mcp2518fd_ns.enum("CanMode")
CAN_MODE = {
    "NORMAL":        CanModeEnum.CAN_MODE_NORMAL,
    "LISTEN_ONLY":   CanModeEnum.CAN_MODE_LISTEN_ONLY,
    "LOOPBACK_INT":  CanModeEnum.CAN_MODE_LOOPBACK_INT,
    "LOOPBACK_EXT":  CanModeEnum.CAN_MODE_LOOPBACK_EXT,
    "SLEEP":         CanModeEnum.CAN_MODE_SLEEP,
    "CLASSIC":       CanModeEnum.CAN_MODE_CLASSIC,
    "RESTRICTED":    CanModeEnum.CAN_MODE_RESTRICTED,
}

# ------------------------------------------------------------------
# YAML configuration keys
# (same names as mcp2515 where applicable)
# ------------------------------------------------------------------
CONF_MCP_CLOCK     = "mcp_clock"
CONF_MCP_MODE      = "mcp_mode"
CONF_CANFD_ENABLED = "canfd_enabled"
CONF_DATA_RATE     = "data_rate"

# ------------------------------------------------------------------
# Schema — extends canbus_schema() exactly as mcp2515 does
# ------------------------------------------------------------------
CONFIG_SCHEMA = (
    canbus.canbus_schema(MCP2518FD)
    .extend(
        {
            cv.Optional(CONF_MCP_CLOCK, default="40MHz"):
                cv.enum(CAN_CLOCK),
            cv.Optional(CONF_MCP_MODE, default="NORMAL"):
                cv.enum(CAN_MODE),
            cv.Optional(CONF_CANFD_ENABLED, default=False):
                cv.boolean,
            cv.Optional(CONF_DATA_RATE, default="500kbps"):
                cv.enum(canbus.CAN_SPEED, upper=True),
        }
    )
    .extend(spi.spi_device_schema(cs_pin_required=True))
)

# ------------------------------------------------------------------
# Code generation
# ------------------------------------------------------------------
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await canbus.register_canbus(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_mcp_clock(config[CONF_MCP_CLOCK]))
    cg.add(var.set_mcp_mode(config[CONF_MCP_MODE]))
    cg.add(var.set_canfd_enabled(config[CONF_CANFD_ENABLED]))
    cg.add(var.set_data_rate(config[CONF_DATA_RATE]))
