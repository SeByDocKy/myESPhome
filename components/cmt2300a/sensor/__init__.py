import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from .. import CMT2300AComponent, cmt2300a_ns

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["cmt2300a"]

CONF_CMT2300A_ID = "cmt2300a_id"
CONF_DUTY_CYCLE = "duty_cycle"

CMT2300ADutyCycleSensor = cmt2300a_ns.class_(
    "CMT2300ADutyCycleSensor", sensor.Sensor, cg.PollingComponent
)

_DUTY_CYCLE_SCHEMA = sensor.sensor_schema(
    CMT2300ADutyCycleSensor,
    unit_of_measurement=UNIT_PERCENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:radio-tower",
).extend(cv.polling_component_schema("60s"))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CMT2300A_ID): cv.use_id(CMT2300AComponent),
        cv.Optional(CONF_DUTY_CYCLE): _DUTY_CYCLE_SCHEMA,
    }
)


async def to_code(config):
    radio = await cg.get_variable(config[CONF_CMT2300A_ID])

    if CONF_DUTY_CYCLE in config:
        conf = config[CONF_DUTY_CYCLE]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(radio))
