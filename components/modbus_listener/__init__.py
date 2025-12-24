import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_ADDRESS

DEPENDENCIES = ['uart']
AUTO_LOAD = ['text_sensor']
CODEOWNERS = ['@yourusername']
MULTI_CONF = True

# Namespace
modbus_listener_ns = cg.esphome_ns.namespace('modbus_listener')

# Classe principale du Hub
ModbusListenerHub = modbus_listener_ns.class_('ModbusListenerHub', cg.Component, uart.UARTDevice)

# Export pour les sous-composants
CONF_MODBUS_LISTENER_ID = 'modbus_listener_id'

# Options de formatage
CONF_USE_COMMA = 'use_comma'
CONF_USE_HEXA = 'use_hexa'
CONF_USE_BRACKET = 'use_bracket'
CONF_USE_QUOTE = 'use_quote'

# Schema de configuration
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ModbusListenerHub),
    cv.Optional(CONF_ADDRESS): cv.int_range(min=1, max=247),
    cv.Optional(CONF_USE_COMMA, default=False): cv.boolean,
    cv.Optional(CONF_USE_HEXA, default=False): cv.boolean,
    cv.Optional(CONF_USE_BRACKET, default=False): cv.boolean,
    cv.Optional(CONF_USE_QUOTE, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    
    if CONF_ADDRESS in config:
        cg.add(var.set_slave_address(config[CONF_ADDRESS]))
    
    cg.add(var.set_use_comma(config[CONF_USE_COMMA]))
    cg.add(var.set_use_hexa(config[CONF_USE_HEXA]))
    cg.add(var.set_use_bracket(config[CONF_USE_BRACKET]))
    cg.add(var.set_use_quote(config[CONF_USE_QUOTE]))
