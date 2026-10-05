import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

from .. import CONF_SMARTSOLAR_ID, SmartSolar, smartsolar_ns

DEPENDENCIES = ["smartsolar"]

SmartSolarSensor = smartsolar_ns.class_(
    "SmartSolarSensor", sensor.Sensor, cg.Parented.template(SmartSolar)
)

# key -> (SensorKind value, schema). The numeric kinds mirror the C++ enum SensorKind in smartsolar.h.
SENSORS = {
    "battery_voltage": (
        0,
        dict(unit_of_measurement=UNIT_VOLT, accuracy_decimals=2, device_class=DEVICE_CLASS_VOLTAGE,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:battery-charging"),
    ),
    "battery_current": (
        1,
        dict(unit_of_measurement=UNIT_AMPERE, accuracy_decimals=1, device_class=DEVICE_CLASS_CURRENT,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:current-dc"),
    ),
    "battery_power": (
        2,
        dict(unit_of_measurement=UNIT_WATT, accuracy_decimals=0, device_class=DEVICE_CLASS_POWER,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:flash"),
    ),
    "battery_temperature": (
        3,
        dict(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:thermometer"),
    ),
    "pv_voltage": (
        4,
        dict(unit_of_measurement=UNIT_VOLT, accuracy_decimals=2, device_class=DEVICE_CLASS_VOLTAGE,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:solar-panel"),
    ),
    "pv_current": (
        5,
        dict(unit_of_measurement=UNIT_AMPERE, accuracy_decimals=1, device_class=DEVICE_CLASS_CURRENT,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:current-dc"),
    ),
    "pv_power": (
        6,
        dict(unit_of_measurement=UNIT_WATT, accuracy_decimals=0, device_class=DEVICE_CLASS_POWER,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:solar-power"),
    ),
    "energy_today": (
        7,
        dict(unit_of_measurement=UNIT_KILOWATT_HOURS, accuracy_decimals=2, device_class=DEVICE_CLASS_ENERGY,
             state_class=STATE_CLASS_TOTAL_INCREASING, icon="mdi:solar-power"),
    ),
    "energy_yesterday": (
        8,
        dict(unit_of_measurement=UNIT_KILOWATT_HOURS, accuracy_decimals=2, device_class=DEVICE_CLASS_ENERGY,
             icon="mdi:solar-power"),
    ),
    "energy_total": (
        9,
        dict(unit_of_measurement=UNIT_KILOWATT_HOURS, accuracy_decimals=2, device_class=DEVICE_CLASS_ENERGY,
             state_class=STATE_CLASS_TOTAL_INCREASING, icon="mdi:solar-power"),
    ),
    "max_power_today": (
        10,
        dict(unit_of_measurement=UNIT_WATT, accuracy_decimals=0, device_class=DEVICE_CLASS_POWER,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:flash"),
    ),
    "max_power_yesterday": (
        11,
        dict(unit_of_measurement=UNIT_WATT, accuracy_decimals=0, device_class=DEVICE_CLASS_POWER,
             icon="mdi:flash"),
    ),
    "internal_temperature": (
        12,
        dict(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE,
             state_class=STATE_CLASS_MEASUREMENT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
             icon="mdi:thermometer"),
    ),
    "absorption_voltage": (
        13,
        dict(unit_of_measurement=UNIT_VOLT, accuracy_decimals=2, device_class=DEVICE_CLASS_VOLTAGE,
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:battery-charging-high"),
    ),
    "float_voltage": (
        14,
        dict(unit_of_measurement=UNIT_VOLT, accuracy_decimals=2, device_class=DEVICE_CLASS_VOLTAGE,
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:battery-charging-medium"),
    ),
    "max_charge_current": (
        15,
        dict(unit_of_measurement=UNIT_AMPERE, accuracy_decimals=1, device_class=DEVICE_CLASS_CURRENT,
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:current-dc"),
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SMARTSOLAR_ID): cv.use_id(SmartSolar),
        **{
            cv.Optional(key): sensor.sensor_schema(SmartSolarSensor, **kwargs)
            for key, (_, kwargs) in SENSORS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SMARTSOLAR_ID])
    for key, (kind, _) in SENSORS.items():
        if key in config:
            var = await sensor.new_sensor(config[key])
            await cg.register_parented(var, config[CONF_SMARTSOLAR_ID])
            cg.add(var.set_kind(kind))
            cg.add(hub.register_sensor(var))
