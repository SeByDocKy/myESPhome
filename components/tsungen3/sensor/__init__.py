import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TEMPERATURE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

from .. import CONF_TSUNGEN3_ID, TSunGen3Component

DEPENDENCIES = ["tsungen3"]

CONF_GRID_VOLTAGE = "grid_voltage"
CONF_GRID_CURRENT = "grid_current"
CONF_GRID_FREQUENCY = "grid_frequency"
CONF_RATED_POWER = "rated_power"
CONF_CURRENT_POWER = "current_power"
CONF_AC_DAILY_ENERGY = "ac_daily_energy"
CONF_AC_TOTAL_ENERGY = "ac_total_energy"

CONF_PV1_VOLTAGE = "pv1_voltage"
CONF_PV1_CURRENT = "pv1_current"
CONF_PV1_POWER = "pv1_power"
CONF_PV2_VOLTAGE = "pv2_voltage"
CONF_PV2_CURRENT = "pv2_current"
CONF_PV2_POWER = "pv2_power"
CONF_PV3_VOLTAGE = "pv3_voltage"
CONF_PV3_CURRENT = "pv3_current"
CONF_PV3_POWER = "pv3_power"
CONF_PV4_VOLTAGE = "pv4_voltage"
CONF_PV4_CURRENT = "pv4_current"
CONF_PV4_POWER = "pv4_power"

# Icon/accuracy conventions match this author's `hms` component
# (sensor/__init__.py: _POWER_SCHEMA, _DC_CURRENT_SCHEMA, _AC_CURRENT_SCHEMA,
# _VOLTAGE_SCHEMA, _ENERGY_TODAY_SCHEMA/_ENERGY_TOTAL_SCHEMA, _FREQUENCY_SCHEMA,
# _TEMPERATURE_SCHEMA) -- same icon strings and accuracy_decimals values.
_POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:power",
)
# Grid-side (AC) current
_AC_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="A",
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:current-ac",
)
# PV-side (DC) current
_DC_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="A",
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:current-dc",
)
# Shared by grid (AC) and PV (DC) voltage, same as hms's single _VOLTAGE_SCHEMA
_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:sine-wave",
)
_ENERGY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOWATT_HOURS,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    accuracy_decimals=3,
    icon="mdi:counter",
)
_FREQUENCY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:metronome",
)
_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:thermometer",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSUNGEN3_ID): cv.use_id(TSunGen3Component),
        cv.Optional(CONF_GRID_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_GRID_CURRENT): _AC_CURRENT_SCHEMA,
        cv.Optional(CONF_GRID_FREQUENCY): _FREQUENCY_SCHEMA,
        cv.Optional(CONF_TEMPERATURE): _TEMPERATURE_SCHEMA,
        cv.Optional(CONF_RATED_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_CURRENT_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_AC_DAILY_ENERGY): _ENERGY_SCHEMA,
        cv.Optional(CONF_AC_TOTAL_ENERGY): _ENERGY_SCHEMA,
        cv.Optional(CONF_PV1_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_PV1_CURRENT): _DC_CURRENT_SCHEMA,
        cv.Optional(CONF_PV1_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_PV2_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_PV2_CURRENT): _DC_CURRENT_SCHEMA,
        cv.Optional(CONF_PV2_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_PV3_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_PV3_CURRENT): _DC_CURRENT_SCHEMA,
        cv.Optional(CONF_PV3_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_PV4_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_PV4_CURRENT): _DC_CURRENT_SCHEMA,
        cv.Optional(CONF_PV4_POWER): _POWER_SCHEMA,
    }
)

_SETTERS = {
    CONF_GRID_VOLTAGE: "set_grid_voltage_sensor",
    CONF_GRID_CURRENT: "set_grid_current_sensor",
    CONF_GRID_FREQUENCY: "set_grid_frequency_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_RATED_POWER: "set_rated_power_sensor",
    CONF_CURRENT_POWER: "set_current_power_sensor",
    CONF_AC_DAILY_ENERGY: "set_ac_daily_energy_sensor",
    CONF_AC_TOTAL_ENERGY: "set_ac_total_energy_sensor",
    CONF_PV1_VOLTAGE: "set_pv1_voltage_sensor",
    CONF_PV1_CURRENT: "set_pv1_current_sensor",
    CONF_PV1_POWER: "set_pv1_power_sensor",
    CONF_PV2_VOLTAGE: "set_pv2_voltage_sensor",
    CONF_PV2_CURRENT: "set_pv2_current_sensor",
    CONF_PV2_POWER: "set_pv2_power_sensor",
    CONF_PV3_VOLTAGE: "set_pv3_voltage_sensor",
    CONF_PV3_CURRENT: "set_pv3_current_sensor",
    CONF_PV3_POWER: "set_pv3_power_sensor",
    CONF_PV4_VOLTAGE: "set_pv4_voltage_sensor",
    CONF_PV4_CURRENT: "set_pv4_current_sensor",
    CONF_PV4_POWER: "set_pv4_power_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_TSUNGEN3_ID])
    for key, setter in _SETTERS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(hub, setter)(sens))
