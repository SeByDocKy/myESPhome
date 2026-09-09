import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from .. import NRF24Component, nrf24l01_ns

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["nrf24l01"]

CONF_NRF24L01_ID = "nrf24l01_id"
CONF_DUTY_CYCLE = "duty_cycle"

NRF24DutyCycleSensor = nrf24l01_ns.class_(
    "NRF24DutyCycleSensor", sensor.Sensor, cg.PollingComponent
)

_DUTY_CYCLE_SCHEMA = sensor.sensor_schema(
    NRF24DutyCycleSensor,
    unit_of_measurement=UNIT_PERCENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:radio-tower",
).extend(cv.polling_component_schema("60s"))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NRF24L01_ID): cv.use_id(NRF24Component),
        cv.Optional(CONF_DUTY_CYCLE): _DUTY_CYCLE_SCHEMA,
    }
)


async def to_code(config):
    radio = await cg.get_variable(config[CONF_NRF24L01_ID])

    if CONF_DUTY_CYCLE in config:
        conf = config[CONF_DUTY_CYCLE]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(radio))
