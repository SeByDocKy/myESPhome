import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_ADDRESS

# No AUTO_LOAD on purpose: the sensor / binary_sensor domains are only loaded
# when the YAML really declares such a platform. This keeps the USE_SENSOR /
# USE_BINARY_SENSOR defines accurate, which the C++ guards rely on.
DEPENDENCIES = ['uart']
CODEOWNERS = ['@yourusername']
MULTI_CONF = True

# Namespace
modbus_sniffer_ns = cg.esphome_ns.namespace('modbus_sniffer')

# Classe principale du Hub
ModbusSnifferHub = modbus_sniffer_ns.class_('ModbusSnifferHub', cg.Component, uart.UARTDevice)

# Export pour les sous-composants
CONF_MODBUS_SNIFFER_ID = 'modbus_sniffer_id'

# Schema de configuration
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ModbusSnifferHub),
    cv.Optional(CONF_ADDRESS): cv.int_range(min=1, max=247),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    
    if CONF_ADDRESS in config:
        cg.add(var.set_slave_address(config[CONF_ADDRESS]))