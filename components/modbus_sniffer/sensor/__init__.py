import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
)
from .. import modbus_sniffer_ns, ModbusSnifferHub, CONF_MODBUS_SNIFFER_ID

DEPENDENCIES = ['modbus_sniffer']

# Classe du sensor (définie dans sensor/)
ModbusSnifferSensor = modbus_sniffer_ns.class_('ModbusSnifferSensor', sensor.Sensor, cg.Component)

CONF_VALUE_TYPE = 'value_type'
CONF_REGISTER_TYPE = 'register_type'

# Énumérations (doivent correspondre au .h)
ValueType = modbus_sniffer_ns.enum('ValueType')
VALUE_TYPES = {
    'U_WORD': ValueType.U_WORD,
    'S_WORD': ValueType.S_WORD,
    'U_DWORD': ValueType.U_DWORD,
    'S_DWORD': ValueType.S_DWORD,
    'U_DWORD_R': ValueType.U_DWORD_R,
    'S_DWORD_R': ValueType.S_DWORD_R,
    'FP32': ValueType.FP32,
    'FP32_R': ValueType.FP32_R,
}

RegisterType = modbus_sniffer_ns.enum('RegisterType')
REGISTER_TYPES = {
    'holding': RegisterType.HOLDING,
    'input': RegisterType.INPUT,
}

CONFIG_SCHEMA = sensor.sensor_schema(
    ModbusSnifferSensor,
).extend({
    cv.GenerateID(CONF_MODBUS_SNIFFER_ID): cv.use_id(ModbusSnifferHub),
    cv.Required(CONF_ADDRESS): cv.hex_int_range(min=0, max=0xFFFF),
    cv.Required(CONF_VALUE_TYPE): cv.enum(VALUE_TYPES, upper=True),
    cv.Optional(CONF_REGISTER_TYPE, default='holding'): cv.enum(REGISTER_TYPES, lower=True),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    
    parent = await cg.get_variable(config[CONF_MODBUS_SNIFFER_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_register_address(config[CONF_ADDRESS]))
    cg.add(var.set_value_type(config[CONF_VALUE_TYPE]))
    cg.add(var.set_register_type(config[CONF_REGISTER_TYPE]))
    cg.add(parent.register_sensor(var))