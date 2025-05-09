import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_CURRENT,
    UNIT_AMPERE,
)

DEPENDENCIES = ["offsr"]
CONF_OUTPUT = "output"
CONF_ERROR  = "error"
CONF_TARGET = "target"
ICON_CURRENT_DC = "mdi:current-dc"

from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

ErrorSensor = offsr_ns.class_("ErrorSensor", sensor.Sensor , cg.Component)
OutputSensor = offsr_ns.class_("OutputSensor", sensor.Sensor , cg.Component)
TargetSensor = offsr_ns.class_("TargetSensor", sensor.Sensor , cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
                
    cv.Optional(CONF_ERROR): sensor.sensor_schema(
                ErrorSensor,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_OUTPUT): sensor.sensor_schema(
                OutputSensor,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_TARGET): sensor.sensor_schema(
                TargetSensor,
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_CURRENT_DC,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
             ),         
}

async def to_code(config):
    offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    if CONF_ERROR in config:
        sens = await sensor.new_sensor(config[CONF_ERROR])
        # await cg.register_component(sens, config)
        # cg.add(sens.set_parent(offsr_component))
        cg.add(offsr_component.set_error_sensor(sens))

    if CONF_OUTPUT in config:
        sens = await sensor.new_sensor(config[CONF_OUTPUT])
        await cg.register_component(sens, config)
        cg.add(offsr_component.set_output_sensor(sens))
        
    if CONF_TARGET in config:
        sens = await sensor.new_sensor(config[CONF_TARGET])
        cg.add(offsr_component.set_target_sensor(sens))    
        