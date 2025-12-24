import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_ADDRESS

DEPENDENCIES = ['uart']
AUTO_LOAD = ['text_sensor']
CODEOWNERS = ['@SeByDocKy,@claude']
MULTI_CONF = True

# Namespace
modbus_listener_ns = cg.esphome_ns.namespace('modbus_listener')

# Classe principale du Hub
ModbusListenerHub = modbus_listener_ns.class_('ModbusListenerHub', cg.Component, uart.UARTDevice)

# Export pour les sous-composants
CONF_MODBUS_LISTENER_ID = 'modbus_listener_id'

# Schema de configuration
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ModbusListenerHub),
    cv.Optional(CONF_ADDRESS): cv.int_range(min=1, max=247),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    
    if CONF_ADDRESS in config:
        cg.add(var.set_slave_address(config[CONF_ADDRESS]))