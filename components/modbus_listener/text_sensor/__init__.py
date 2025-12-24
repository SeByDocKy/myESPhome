import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_NAME

from .. import modbus_listener_ns, ModbusListenerHub, CONF_MODBUS_LISTENER_ID

DEPENDENCIES = ['modbus_listener']

# Classe du text sensor
ModbusListenerTextSensor = modbus_listener_ns.class_(
    'ModbusListenerTextSensor', cg.Component
)

CONF_TX = 'tx'
CONF_RX = 'rx'

def validate_text_sensor_config(config):
    """Valider qu'au moins tx ou rx est présent"""
    if CONF_TX not in config and CONF_RX not in config:
        raise cv.Invalid("At least one of 'tx' or 'rx' must be specified")
    return config

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(ModbusListenerTextSensor),
        cv.GenerateID(CONF_MODBUS_LISTENER_ID): cv.use_id(ModbusListenerHub),
        cv.Optional(CONF_TX): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_RX): text_sensor.text_sensor_schema(),
    }).extend(cv.COMPONENT_SCHEMA),
    validate_text_sensor_config
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    parent = await cg.get_variable(config[CONF_MODBUS_LISTENER_ID])
    cg.add(var.set_parent(parent))
    
    # Configurer le text sensor TX si présent
    if CONF_TX in config:
        tx_sensor = await text_sensor.new_text_sensor(config[CONF_TX])
        cg.add(var.set_tx_sensor(tx_sensor))
    
    # Configurer le text sensor RX si présent
    if CONF_RX in config:
        rx_sensor = await text_sensor.new_text_sensor(config[CONF_RX])
        cg.add(var.set_rx_sensor(rx_sensor))
    
    cg.add(parent.register_text_sensor(var))