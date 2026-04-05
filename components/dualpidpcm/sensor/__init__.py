import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_CURRENT,
    UNIT_AMPERE,
    UNIT_WATT,
    UNIT_PERCENT,
)

DEPENDENCIES = ["dualpidpcm"]

CONF_OUTPUT = "output"
CONF_OUTPUT_CHARGING = "output_charging"
CONF_OUTPUT_DISCHARGING = "output_discharging"
CONF_ERROR  = "error"
CONF_TARGET = "target"
CONF_EPOINT = "epoint"
CONF_INPUT   = "input"

ICON_EPSILON = "mdi:epsilon"
ICON_PERCENT = "mdi:percent"
ICON_TARGET = "mdi:target"
ICON_INPORT = "mdi:import'


from .. import CONF_DUALPIDPCM_ID, DUALPIDPCMComponent, dualpidpcm_ns

DUALPIDPCMSensor = dualpidpcm_ns.class_("DUALPIDPCMSensor", cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(): cv.declare_id(DUALPIDPCMSensor),
    cv.GenerateID(CONF_DUALPIDPCM_ID): cv.use_id(DUALPIDPCMComponent),
    cv.Optional(CONF_ERROR): sensor.sensor_schema(
                accuracy_decimals=2,
                icon = ICON_EPSILON,
                unit_of_measurement=UNIT_WATT,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_OUTPUT): sensor.sensor_schema(
                accuracy_decimals=2,
                unit_of_measurement=UNIT_PERCENT,
                icon = ICON_PERCENT,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
             
    cv.Optional(CONF_OUTPUT_CHARGING): sensor.sensor_schema(
                accuracy_decimals=2,
                unit_of_measurement=UNIT_PERCENT,
                icon = ICON_PERCENT,
                state_class=STATE_CLASS_MEASUREMENT,
             ),

    cv.Optional(CONF_OUTPUT_DISCHARGING): sensor.sensor_schema(
                accuracy_decimals=2,
                unit_of_measurement=UNIT_PERCENT,
                icon = ICON_PERCENT,
                state_class=STATE_CLASS_MEASUREMENT,
             ),             
    cv.Optional(CONF_TARGET): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_TARGET,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
     cv.Optional(CONF_EPOINT): sensor.sensor_schema(
                accuracy_decimals=1,
                unit_of_measurement=UNIT_PERCENT,
                icon = ICON_PERCENT,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
     cv.Optional(CONF_INPUT): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_INPORT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
}

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    dualpid_component = await cg.get_variable(config[CONF_DUALPIDPCM_ID])
    cg.add(var.set_parent(dualpid_component))

    if CONF_ERROR in config:
        sens = await sensor.new_sensor(config[CONF_ERROR])
        cg.add(var.set_error_sensor(sens))

    if CONF_OUTPUT in config:
        sens = await sensor.new_sensor(config[CONF_OUTPUT])
        cg.add(var.set_output_sensor(sens))
        
    if CONF_OUTPUT_CHARGING in config:
        sens = await sensor.new_sensor(config[CONF_OUTPUT_CHARGING])
        cg.add(var.set_output_charging_sensor(sens))

    if CONF_OUTPUT_DISCHARGING in config:
        sens = await sensor.new_sensor(config[CONF_OUTPUT_DISCHARGING])
        cg.add(var.set_output_discharging_sensor(sens))        
        
    if CONF_TARGET in config:
        sens = await sensor.new_sensor(config[CONF_TARGET])
        cg.add(var.set_target_sensor(sens))
   
    if CONF_EPOINT in config:
        sens = await sensor.new_sensor(config[CONF_EPOINT])
        cg.add(var.set_epoint_sensor(sens))

    if CONF_INPUT in config:
        sens = await sensor.new_sensor(config[CONF_INPUT])
        cg.add(var.set_input_sensor(sens))
 
