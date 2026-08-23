from __future__ import annotations

from typing import Literal

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
import esphome.final_validate as fv
from esphome.components import socket

CONF_IP_ADDRESS = 'host'
CONF_PORT = 'port'

def _consume_sockets(config):
    """Register socket needs for this component."""
    # Example: 1 listening socket + 2 concurrent client connections
    socket.consume_sockets(2, "modbustcp")(config)
    return config

modbustcp_ns = cg.esphome_ns.namespace("modbustcp")
ModbusTCP = modbustcp_ns.class_("ModbusTCP", cg.Component)
ModbusDevice = modbustcp_ns.class_("ModbusDevice")

MULTI_CONF = True
AUTO_LOAD = ["async_tcp"]

CONF_MODBUSTCP_ID = "modbustcp_id"
CONF_SEND_WAIT_TIME = "send_wait_time"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusTCP),
            cv.Required(CONF_IP_ADDRESS): cv.ipv4address,
            cv.Optional(CONF_PORT, default=502): cv.int_range(0, 65535),
            cv.Optional(
                CONF_SEND_WAIT_TIME, default="250ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    cg.add_global(modbustcp_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_host(str(config[CONF_IP_ADDRESS])))
    cg.add(var.set_port(config["port"]))
    cg.add(var.set_send_wait_time(config[CONF_SEND_WAIT_TIME]))
    #await uart.register_uart_device(var, config)
    #await tcp.register_uart_device(var, config)
    
def modbus_device_schema(default_address):
    schema = {
        cv.GenerateID(CONF_MODBUSTCP_ID): cv.use_id(ModbusTCP),
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = cv.hex_uint8_t
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = cv.hex_uint8_t
    return cv.Schema(schema)


async def register_modbus_device(var, config):
    parent = await cg.get_variable(config[CONF_MODBUSTCP_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(parent.register_device(var))

async def register_tcp_device(var, config):
    """Register a UART device, setting up all the internal values.

    This is a coroutine, you need to await it with an 'await' expression!
    """
    parent = await cg.get_variable(config[CONF_TCP_ID])
    cg.add(var.set_tcp_parent(parent))
