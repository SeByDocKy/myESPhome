import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
)
from .. import modbus_sniffer_ns, ModbusSnifferHub, CONF_MODBUS_SNIFFER_ID

DEPENDENCIES = ['modbus_sniffer']

# Classe du binary sensor
ModbusSnifferBinarySensor = modbus_sniffer_ns.class_(
    'ModbusSnifferBinarySensor', binary_sensor.BinarySensor, cg.Component
)

CONF_BITMASK = 'bitmask'
CONF_REGISTER_TYPE = 'register_type'

RegisterType = modbus_sniffer_ns.enum('RegisterType')
REGISTER_TYPES = {
    'holding': RegisterType.HOLDING,
    'read': RegisterType.INPUT,
}

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    ModbusSnifferBinarySensor,
).extend({
    cv.GenerateID(CONF_MODBUS_SNIFFER_ID): cv.use_id(ModbusSnifferHub),
    cv.Required(CONF_ADDRESS): cv.hex_int_range(min=0, max=0xFFFF),
    cv.Required(CONF_BITMASK): cv.hex_int_range(min=0, max=0xFFFF),
    cv.Optional(CONF_REGISTER_TYPE, default='holding'): cv.enum(REGISTER_TYPES, lower=True),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    
    parent = await cg.get_variable(config[CONF_MODBUS_SNIFFER_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_register_address(config[CONF_ADDRESS]))
    cg.add(var.set_bitmask(config[CONF_BITMASK]))
    cg.add(var.set_register_type(config[CONF_REGISTER_TYPE]))
    cg.add(parent.register_binary_sensor(var))