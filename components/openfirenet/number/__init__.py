import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS, UNIT_PERCENT

from .. import CONF_OPENFIRENET_ID, OpenFireNetComponent, openfirenet_ns

DEPENDENCIES = ["openfirenet"]

TargetTemperatureNumber = openfirenet_ns.class_(
    "OpenFireNetTargetTemperatureNumber", number.Number, cg.Parented.template(OpenFireNetComponent)
)
HeatingPowerNumber = openfirenet_ns.class_(
    "OpenFireNetHeatingPowerNumber", number.Number, cg.Parented.template(OpenFireNetComponent)
)

CONF_TARGET_TEMPERATURE = "target_temperature"
CONF_HEATING_POWER = "heating_power"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENFIRENET_ID): cv.use_id(OpenFireNetComponent),
        cv.Optional(CONF_TARGET_TEMPERATURE): number.number_schema(
            TargetTemperatureNumber,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            icon="mdi:thermometer-check",
        ),
        cv.Optional(CONF_HEATING_POWER): number.number_schema(
            HeatingPowerNumber, unit_of_measurement=UNIT_PERCENT, icon="mdi:fire"
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENFIRENET_ID])
    if conf := config.get(CONF_TARGET_TEMPERATURE):
        n = await number.new_number(conf, min_value=14.0, max_value=28.0, step=0.5)
        await cg.register_parented(n, hub)
        cg.add(hub.set_target_temperature_number(n))
    if conf := config.get(CONF_HEATING_POWER):
        n = await number.new_number(conf, min_value=30.0, max_value=100.0, step=1.0)
        await cg.register_parented(n, hub)
        cg.add(hub.set_heating_power_number(n))
