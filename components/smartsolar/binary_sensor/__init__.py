import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_LIGHT, DEVICE_CLASS_PROBLEM

from .. import CONF_SMARTSOLAR_ID, SmartSolar, smartsolar_ns

DEPENDENCIES = ["smartsolar"]

SmartSolarBinarySensor = smartsolar_ns.class_(
    "SmartSolarBinarySensor", binary_sensor.BinarySensor, cg.Parented.template(SmartSolar)
)

# key -> (BinarySensorKind value, schema). The numeric kinds mirror the C++ enum BinarySensorKind in smartsolar.h.
BINARY_SENSORS = {
    "relay": (0, dict(icon="mdi:electric-switch-closed")),
    "alarm": (1, dict(device_class=DEVICE_CLASS_PROBLEM, icon="mdi:alert")),
    "low_voltage": (
        2,
        dict(device_class=DEVICE_CLASS_PROBLEM, icon="mdi:battery-alert-variant-outline"),
    ),
    "high_voltage": (
        3,
        dict(device_class=DEVICE_CLASS_PROBLEM, icon="mdi:battery-alert-variant"),
    ),
    # "on" while the panels are irradiated: usable as a day/night detector (firmware >= 2.01)
    "solar_activity": (4, dict(device_class=DEVICE_CLASS_LIGHT, icon="mdi:white-balance-sunny")),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SMARTSOLAR_ID): cv.use_id(SmartSolar),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema(SmartSolarBinarySensor, **kwargs)
            for key, (_, kwargs) in BINARY_SENSORS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SMARTSOLAR_ID])
    for key, (kind, _) in BINARY_SENSORS.items():
        if key in config:
            var = await binary_sensor.new_binary_sensor(config[key])
            await cg.register_parented(var, config[CONF_SMARTSOLAR_ID])
            cg.add(var.set_kind(kind))
            cg.add(hub.register_binary_sensor(var))
