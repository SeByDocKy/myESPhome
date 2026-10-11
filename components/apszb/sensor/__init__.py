import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_SECOND,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

from .. import APSInverter, CONF_INVERTER

DEPENDENCIES = ["apszb"]

CONF_AC_VOLTAGE = "ac_voltage"
CONF_FREQUENCY = "frequency"
CONF_TEMPERATURE = "temperature"
CONF_LINK_QUALITY = "link_quality"
CONF_RSSI = "rssi"
CONF_POWER_TOTAL = "power_total"
CONF_ENERGY_TOTAL = "energy_total"
CONF_UPTIME = "uptime"

CHANNELS = (1, 2, 3, 4)

# key -> (schema, hub setter)
SINGLE_SENSORS = {
    CONF_AC_VOLTAGE: (
        sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:sine-wave",
        ),
        "set_ac_voltage_sensor",
    ),
    CONF_FREQUENCY: (
        sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:current-ac",
        ),
        "set_frequency_sensor",
    ),
    CONF_TEMPERATURE: (
        sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:thermometer",
        ),
        "set_temperature_sensor",
    ),
    CONF_LINK_QUALITY: (
        sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:signal",
        ),
        "set_link_quality_sensor",
    ),
    CONF_RSSI: (
        sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:wifi-strength-2",
        ),
        "set_rssi_sensor",
    ),
    CONF_POWER_TOTAL: (
        sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:solar-power",
        ),
        "set_power_total_sensor",
    ),
    CONF_ENERGY_TOTAL: (
        sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT_HOURS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon="mdi:counter",
        ),
        "set_energy_total_sensor",
    ),
    CONF_UPTIME: (
        sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:timer-outline",
        ),
        "set_uptime_sensor",
    ),
}

# prefix -> (schema factory, hub setter); registered as <prefix>_1 ... <prefix>_4
CHANNEL_SENSORS = {
    "dc_voltage": (
        lambda: sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:solar-panel",
        ),
        "set_dc_voltage_sensor",
    ),
    "dc_current": (
        lambda: sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:current-dc",
        ),
        "set_dc_current_sensor",
    ),
    "power": (
        lambda: sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:solar-power",
        ),
        "set_power_sensor",
    ),
    "energy": (
        lambda: sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT_HOURS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon="mdi:counter",
        ),
        "set_energy_sensor",
    ),
}

_schema = {cv.Required(CONF_INVERTER): cv.use_id(APSInverter)}
for _key, (_sch, _setter) in SINGLE_SENSORS.items():
    _schema[cv.Optional(_key)] = _sch
for _prefix, (_factory, _setter) in CHANNEL_SENSORS.items():
    for _ch in CHANNELS:
        _schema[cv.Optional(f"{_prefix}_{_ch}")] = _factory()

CONFIG_SCHEMA = cv.Schema(_schema)


async def to_code(config):
    inv = await cg.get_variable(config[CONF_INVERTER])

    for key, (_, setter) in SINGLE_SENSORS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(inv, setter)(sens))

    for prefix, (_, setter) in CHANNEL_SENSORS.items():
        for ch in CHANNELS:
            key = f"{prefix}_{ch}"
            if key in config:
                sens = await sensor.new_sensor(config[key])
                cg.add(getattr(inv, setter)(ch - 1, sens))
