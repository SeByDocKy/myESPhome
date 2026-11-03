from logging import getLogger

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_RX_BUFFER_SIZE, CONF_UART_ID
import esphome.final_validate as fv

_LOGGER = getLogger(__name__)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]
MULTI_CONF = True

victron_smart_shunt_ns = cg.esphome_ns.namespace("victron_smart_shunt")
VictronSmartShuntComponent = victron_smart_shunt_ns.class_(
    "VictronSmartShuntComponent", uart.UARTDevice, cg.Component
)

CONF_VICTRON_SMART_SHUNT_ID = "victron_smart_shunt_id"

# A SmartShunt text block is ~250-300 bytes sent in a burst at 19200 baud. With the default 256 bytes
# RX buffer of a native ESP UART, a loop() stalled for ~130 ms (Wi-Fi reconnect, slow component...)
# overflows it. This only applies to native UARTs: a UART expander channel (weikai...) has no
# rx_buffer_size option, its buffering is fixed by the expander component.
RECOMMENDED_RX_BUFFER_SIZE = 512

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(VictronSmartShuntComponent),
    }
)


def _final_validate(config):
    full_config = fv.full_config.get()
    uart_path = full_config.get_path_for_id(config[CONF_UART_ID])[:-1]
    uart_config = full_config.get_config_for_path(uart_path)
    uart_id = uart_config.get(CONF_ID)
    if uart_id is None or str(uart_id.type) not in uart.NATIVE_UART_CLASSES:
        return config
    rx_buffer_size = uart_config.get(CONF_RX_BUFFER_SIZE, 256)
    if rx_buffer_size < RECOMMENDED_RX_BUFFER_SIZE:
        _LOGGER.warning(
            "victron_smart_shunt: the uart rx_buffer_size is %d bytes, at least %d is recommended. "
            "If the main loop is blocked for more than ~130 ms, bytes are lost and a VE.Direct "
            "block is corrupted. Add 'rx_buffer_size: 1024' to your uart: configuration.",
            rx_buffer_size,
            RECOMMENDED_RX_BUFFER_SIZE,
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
