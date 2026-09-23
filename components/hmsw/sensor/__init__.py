import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_CURRENT,
    CONF_FREQUENCY,
    CONF_POWER,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_DECIBEL,
    UNIT_EMPTY,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

from .. import HMSWComponent

CONF_HMSW_ID = "hmsw_id"
CONF_DC_CHANNELS = "dc_channels"
CONF_AC = "ac"
CONF_ENERGY_TOTAL = "energy_total"
CONF_POWER_FACTOR = "power_factor"
CONF_REACTIVE_POWER = "reactive_power"
CONF_RSSI = "rssi"

# RealDataNew (0xA3 0x11) only -- stay unpublished when data_source:
# real_data (the default) is in use. See README.md.
CONF_ENERGY_DAILY = "energy_daily"
CONF_POWER_LIMIT = "power_limit"
CONF_WARNING_NUMBER = "warning_number"
CONF_LINK_STATUS = "link_status"

DEPENDENCIES = ["hmsw"]

_POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:power",
)
_DC_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="A",
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:current-dc",
)
_AC_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="A",
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:current-ac",
)
_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:sine-wave",
)
_ENERGY_TOTAL_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT_HOURS,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    accuracy_decimals=0,
    icon="mdi:counter",
)
_FREQUENCY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:metronome",
)
_POWER_FACTOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_EMPTY,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=3,
    icon="mdi:angle-acute",
)
_REACTIVE_POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:power",
)
_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:thermometer",
)
_RSSI_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_DECIBEL,
    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=0,
    icon="mdi:wifi",
)
# Daily energy resets at midnight (on the inverter's own clock), so
# "total" (not "total_increasing", which HA expects to only ever grow) --
# same convention HA uses for other "today" energy sensors.
_ENERGY_DAILY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT_HOURS,
    device_class=DEVICE_CLASS_ENERGY,
    state_class="total",
    accuracy_decimals=0,
    icon="mdi:counter",
)
# Readback of the currently-applied power limit (SGSMO.power_limit) --
# RealDataNew only, no equivalent in classic RealData. Exposed as a raw
# diagnostic sensor, not a percentage: whether the firmware reports this in
# Watts or some other scale is unverified against real hardware -- see
# README.md and the field-scaling comment in handle_real_data_new_().
_POWER_LIMIT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category="diagnostic",
    accuracy_decimals=1,
    icon="mdi:speedometer",
)
_WARNING_NUMBER_SCHEMA = sensor.sensor_schema(
    entity_category="diagnostic",
    accuracy_decimals=0,
    icon="mdi:alert",
)
# Raw firmware value, meaning/range not yet confirmed against real hardware
# -- deliberately not mapped to a binary_sensor until that's known. See
# README.md.
_LINK_STATUS_SCHEMA = sensor.sensor_schema(
    entity_category="diagnostic",
    accuracy_decimals=0,
    icon="mdi:link-variant",
)

DC_CHANNEL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_CURRENT): _DC_CURRENT_SCHEMA,
        cv.Optional(CONF_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_ENERGY_TOTAL): _ENERGY_TOTAL_SCHEMA,
        cv.Optional(CONF_TEMPERATURE): _TEMPERATURE_SCHEMA,
        cv.Optional(CONF_ENERGY_DAILY): _ENERGY_DAILY_SCHEMA,
    }
)

AC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_CURRENT): _AC_CURRENT_SCHEMA,
        cv.Optional(CONF_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_FREQUENCY): _FREQUENCY_SCHEMA,
        cv.Optional(CONF_POWER_FACTOR): _POWER_FACTOR_SCHEMA,
        cv.Optional(CONF_REACTIVE_POWER): _REACTIVE_POWER_SCHEMA,
    }
)


def _dc_channel_entry(value):
    value = cv.Schema({cv.string: DC_CHANNEL_SCHEMA})(value)
    if len(value) != 1:
        raise cv.Invalid(
            "Each 'dc_channels' entry must contain exactly one channel name "
            "(e.g. 'pv0: {power: {name: ...}}')."
        )
    return value


def _validate_dc_channels_list(value):
    value = cv.ensure_list(_dc_channel_entry)(value)
    cv.Length(min=1, max=4)(value)

    seen = set()
    for entry in value:
        label = next(iter(entry))
        if label in seen:
            raise cv.Invalid(
                f"Channel name '{label}' is used more than once in "
                f"'dc_channels' -- each channel needs a unique name (pv0, pv1, ...)."
            )
        seen.add(label)
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMSW_ID): cv.use_id(HMSWComponent),
        cv.Optional(CONF_DC_CHANNELS): _validate_dc_channels_list,
        cv.Optional(CONF_AC): AC_SCHEMA,
        cv.Optional(CONF_RSSI): _RSSI_SCHEMA,
        cv.Optional(CONF_POWER_LIMIT): _POWER_LIMIT_SCHEMA,
        cv.Optional(CONF_WARNING_NUMBER): _WARNING_NUMBER_SCHEMA,
        cv.Optional(CONF_LINK_STATUS): _LINK_STATUS_SCHEMA,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMSW_ID])

    for i, entry in enumerate(config.get(CONF_DC_CHANNELS, [])):
        _label, channel = next(iter(entry.items()))
        if CONF_POWER in channel:
            s = await sensor.new_sensor(channel[CONF_POWER])
            cg.add(hub.set_dc_power_sensor(i, s))
        if CONF_CURRENT in channel:
            s = await sensor.new_sensor(channel[CONF_CURRENT])
            cg.add(hub.set_dc_current_sensor(i, s))
        if CONF_VOLTAGE in channel:
            s = await sensor.new_sensor(channel[CONF_VOLTAGE])
            cg.add(hub.set_dc_voltage_sensor(i, s))
        if CONF_ENERGY_TOTAL in channel:
            s = await sensor.new_sensor(channel[CONF_ENERGY_TOTAL])
            cg.add(hub.set_dc_energy_total_sensor(i, s))
        if CONF_TEMPERATURE in channel:
            s = await sensor.new_sensor(channel[CONF_TEMPERATURE])
            cg.add(hub.set_dc_temperature_sensor(i, s))
        if CONF_ENERGY_DAILY in channel:
            s = await sensor.new_sensor(channel[CONF_ENERGY_DAILY])
            cg.add(hub.set_dc_energy_daily_sensor(i, s))

    if CONF_AC in config:
        ac = config[CONF_AC]
        if CONF_VOLTAGE in ac:
            s = await sensor.new_sensor(ac[CONF_VOLTAGE])
            cg.add(hub.set_ac_voltage_sensor(s))
        if CONF_CURRENT in ac:
            s = await sensor.new_sensor(ac[CONF_CURRENT])
            cg.add(hub.set_ac_current_sensor(s))
        if CONF_POWER in ac:
            s = await sensor.new_sensor(ac[CONF_POWER])
            cg.add(hub.set_ac_power_sensor(s))
        if CONF_FREQUENCY in ac:
            s = await sensor.new_sensor(ac[CONF_FREQUENCY])
            cg.add(hub.set_ac_frequency_sensor(s))
        if CONF_POWER_FACTOR in ac:
            s = await sensor.new_sensor(ac[CONF_POWER_FACTOR])
            cg.add(hub.set_ac_power_factor_sensor(s))
        if CONF_REACTIVE_POWER in ac:
            s = await sensor.new_sensor(ac[CONF_REACTIVE_POWER])
            cg.add(hub.set_ac_reactive_power_sensor(s))

    if CONF_RSSI in config:
        s = await sensor.new_sensor(config[CONF_RSSI])
        cg.add(hub.set_rssi_sensor(s))

    if CONF_POWER_LIMIT in config:
        s = await sensor.new_sensor(config[CONF_POWER_LIMIT])
        cg.add(hub.set_power_limit_readback_sensor(s))

    if CONF_WARNING_NUMBER in config:
        s = await sensor.new_sensor(config[CONF_WARNING_NUMBER])
        cg.add(hub.set_warning_number_sensor(s))

    if CONF_LINK_STATUS in config:
        s = await sensor.new_sensor(config[CONF_LINK_STATUS])
        cg.add(hub.set_link_status_sensor(s))
