import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    UNIT_AMPERE,
    UNIT_VOLT,
)

DEPENDENCIES = ["dmtcp"]

CONF_PV1_VOLTAGE = "pv1_voltage"


from .. import CONF_DMTCP_ID, DMTCPComponent, dmtcp_ns

DMTCPSensor = dmtcp_ns.class_("DMTCPSensor", cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(): cv.declare_id(DMTCPSensor),
    
    cv.GenerateID(CONF_DMTCP_ID): cv.use_id(DMTCPComponent),
                
    cv.Optional(CONF_PV1_VOLTAGE): sensor.sensor_schema(
                accuracy_decimals=1,
                unit_of_measurement=UNIT_VOLT,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
             ),  
}

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    dmtcp_component = await cg.get_variable(config[CONF_DMTCP_ID])
    cg.add(var.set_parent(dmtcp_component))

    if CONF_PV1_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_PV1_VOLTAGE])
        cg.add(var.set_pv1_voltage_sensor(sens))
