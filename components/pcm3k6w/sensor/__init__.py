import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
)

from .. import CONF_PCM3K6W_ID, PCM3K6W_PLATFORM_SCHEMA, pcm3k6w_ns

DEPENDENCIES = ["canbus"]

PCM3K6WSensor = pcm3k6w_ns.class_("PCM3K6WSensor", sensor.Sensor)

# Kind indices MUST match the SensorKind enum order in pcm3k6w.h.
(
    KIND_GRID_VOLTAGE,
    KIND_INVERTER_VOLTAGE,
    KIND_BUS_VOLTAGE,
    KIND_INVERTING_CURRENT,
    KIND_DC_VOLTAGE,
    KIND_DC_CURRENT,
    KIND_LOAD_CURRENT,
    KIND_AC_POWER,
    KIND_DC_POWER,
    KIND_TEMPERATURE_PFC,
    KIND_TEMPERATURE_LLC,
    KIND_RUNNING_MODE_VALUE,
    KIND_OFFGRID_FREQUENCY,
    KIND_FREQUENCY_SETTING_READBACK,
    KIND_AC_VOLTAGE_SETTING_READBACK,
    KIND_PHASE_MODE_READBACK,
    KIND_CHARGING_VOLTAGE_READBACK,
    KIND_CHARGING_CURRENT_READBACK,
    KIND_DISCHARGING_CURRENT_READBACK,
    KIND_AC_OVERVOLTAGE_PROTECTION_READBACK,
    KIND_AC_UNDERVOLTAGE_PROTECTION_READBACK,
    KIND_AC_OVERVOLTAGE_ALARM_READBACK,
    KIND_AC_UNDERVOLTAGE_ALARM_READBACK,
    KIND_DC_OVERVOLTAGE_PROTECTION_READBACK,
    KIND_DC_UNDERVOLTAGE_PROTECTION_READBACK,
    KIND_DC_OVERVOLTAGE_ALARM_READBACK,
    KIND_DC_UNDERVOLTAGE_ALARM_READBACK,
    KIND_FAN_SPEED_READBACK,
    KIND_GRID_MODE_READBACK,
) = range(29)

_V = dict(unit_of_measurement=UNIT_VOLT, device_class=DEVICE_CLASS_VOLTAGE, state_class=STATE_CLASS_MEASUREMENT, accuracy_decimals=1)
_A = dict(unit_of_measurement=UNIT_AMPERE, device_class=DEVICE_CLASS_CURRENT, state_class=STATE_CLASS_MEASUREMENT, accuracy_decimals=1)
_W = dict(unit_of_measurement=UNIT_WATT, device_class=DEVICE_CLASS_POWER, state_class=STATE_CLASS_MEASUREMENT, accuracy_decimals=1)
_C = dict(unit_of_measurement=UNIT_CELSIUS, device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT, accuracy_decimals=0)
_HZ = dict(unit_of_measurement=UNIT_HERTZ, device_class=DEVICE_CLASS_FREQUENCY, state_class=STATE_CLASS_MEASUREMENT, accuracy_decimals=0)

# (config key, kind, extra sensor_schema() kwargs)
SENSORS = [
    ("grid_voltage", KIND_GRID_VOLTAGE, {**_V, "icon": "mdi:sine-wave"}),
    ("inverter_voltage", KIND_INVERTER_VOLTAGE, {**_V, "icon": "mdi:sine-wave"}),
    ("bus_voltage", KIND_BUS_VOLTAGE, {**_V, "icon": "mdi:sine-wave"}),
    ("inverting_current", KIND_INVERTING_CURRENT, {**_A, "icon": "mdi:current-ac"}),
    ("dc_voltage", KIND_DC_VOLTAGE, {**_V, "icon": "mdi:sine-wave"}),
    ("dc_current", KIND_DC_CURRENT, {**_A, "icon": "mdi:current-dc"}),
    ("load_current", KIND_LOAD_CURRENT, {**_A, "icon": "mdi:current-ac"}),
    ("ac_power", KIND_AC_POWER, {**_W, "icon": "mdi:power"}),
    ("dc_power", KIND_DC_POWER, dict(_W)),
    ("temperature_pfc", KIND_TEMPERATURE_PFC, {**_C, "icon": "mdi:thermometer"}),
    ("temperature_llc", KIND_TEMPERATURE_LLC, {**_C, "icon": "mdi:thermometer"}),
    ("running_mode_value", KIND_RUNNING_MODE_VALUE, dict(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT, icon="mdi:numeric")),
    ("offgrid_frequency", KIND_OFFGRID_FREQUENCY, {**_HZ, "icon": "mdi:metronome"}),
    ("frequency_setting_readback", KIND_FREQUENCY_SETTING_READBACK, {**_HZ, "icon": "mdi:metronome"}),
    ("ac_voltage_setting_readback", KIND_AC_VOLTAGE_SETTING_READBACK, {**_V, "icon": "mdi:sine-wave"}),
    ("phase_mode_readback", KIND_PHASE_MODE_READBACK, dict(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT, icon="mdi:numeric")),
    ("charging_voltage_readback", KIND_CHARGING_VOLTAGE_READBACK, {**_V, "icon": "mdi:battery-charging-high"}),
    ("charging_current_readback", KIND_CHARGING_CURRENT_READBACK, {**_A, "icon": "mdi:battery-arrow-up"}),
    ("discharging_current_readback", KIND_DISCHARGING_CURRENT_READBACK, {**_A, "icon": "mdi:battery-arrow-down"}),
    ("ac_overvoltage_protection_readback", KIND_AC_OVERVOLTAGE_PROTECTION_READBACK, {**_V, "icon": "mdi:flash-alert"}),
    ("ac_undervoltage_protection_readback", KIND_AC_UNDERVOLTAGE_PROTECTION_READBACK, {**_V, "icon": "mdi:flash-alert-outline"}),
    ("ac_overvoltage_alarm_readback", KIND_AC_OVERVOLTAGE_ALARM_READBACK, {**_V, "icon": "mdi:flash-alert"}),
    ("ac_undervoltage_alarm_readback", KIND_AC_UNDERVOLTAGE_ALARM_READBACK, {**_V, "icon": "mdi:flash-alert-outline"}),
    ("dc_overvoltage_protection_readback", KIND_DC_OVERVOLTAGE_PROTECTION_READBACK, {**_V, "icon": "mdi:battery-alert"}),
    ("dc_undervoltage_protection_readback", KIND_DC_UNDERVOLTAGE_PROTECTION_READBACK, {**_V, "icon": "mdi:battery-alert-variant-outline"}),
    ("dc_overvoltage_alarm_readback", KIND_DC_OVERVOLTAGE_ALARM_READBACK, {**_V, "icon": "mdi:battery-alert"}),
    ("dc_undervoltage_alarm_readback", KIND_DC_UNDERVOLTAGE_ALARM_READBACK, {**_V, "icon": "mdi:battery-alert-variant-outline"}),
    ("fan_speed_readback", KIND_FAN_SPEED_READBACK, dict(unit_of_measurement=UNIT_PERCENT, accuracy_decimals=1, icon="mdi:fan")),
    ("grid_mode_readback", KIND_GRID_MODE_READBACK, dict(accuracy_decimals=0, icon="mdi:transmission-tower", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
]

CONFIG_SCHEMA = PCM3K6W_PLATFORM_SCHEMA.extend(
    {cv.Optional(key): sensor.sensor_schema(PCM3K6WSensor, **kwargs) for key, _kind, kwargs in SENSORS}
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PCM3K6W_ID])
    for key, kind, _kwargs in SENSORS:
        if key in config:
            s = await sensor.new_sensor(config[key])
            cg.add(hub.set_sensor(kind, s))
