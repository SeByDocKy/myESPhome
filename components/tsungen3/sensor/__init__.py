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

# Structure matches this author's hms/hmsw components: a `dc_channels` list
# (one entry per MPPT string, 0-indexed -- pv0, pv1, ...) plus flat `ac:` and
# `inverter:` blocks, instead of the former flat pv1_voltage/pv2_voltage/...
# keys. See README.md "Breaking change" note.
CONF_DC_CHANNELS = "dc_channels"
CONF_AC = "ac"
CONF_INVERTER = "inverter"
CONF_ENERGY_TODAY = "energy_today"
CONF_ENERGY_TOTAL = "energy_total"
CONF_RATED_POWER = "rated_power"

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

DC_CHANNEL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_CURRENT): _DC_CURRENT_SCHEMA,
        cv.Optional(CONF_POWER): _POWER_SCHEMA,
    }
)

AC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_CURRENT): _AC_CURRENT_SCHEMA,
        cv.Optional(CONF_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_FREQUENCY): _FREQUENCY_SCHEMA,
        cv.Optional(CONF_ENERGY_TODAY): _ENERGY_SCHEMA,
        cv.Optional(CONF_ENERGY_TOTAL): _ENERGY_SCHEMA,
    }
)

INVERTER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TEMPERATURE): _TEMPERATURE_SCHEMA,
        cv.Optional(CONF_RATED_POWER): _POWER_SCHEMA,
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
    # No compile-time SN-based cross-check against the actual MPPT count on
    # real hardware (unlike hms, which decodes it from the Hoymiles SN
    # prefix) -- no equivalent prefix table is known for TSUN GEN3 PLUS
    # serials, and this component has no `model:` config key either. Just a
    # generic 1-4 bound for now.
    cv.Length(min=1, max=4)(value)

    seen = set()
    for entry in value:
        label = next(iter(entry))
        if label in seen:
            raise cv.Invalid(
                f"Channel name '{label}' is used more than once in "
                f"'dc_channels' -- each channel must have a unique name (pv0, pv1, ...)."
            )
        seen.add(label)
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSUNGEN3_ID): cv.use_id(TSunGen3Component),
        cv.Optional(CONF_DC_CHANNELS): _validate_dc_channels_list,
        cv.Optional(CONF_AC): AC_SCHEMA,
        cv.Optional(CONF_INVERTER): INVERTER_SCHEMA,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_TSUNGEN3_ID])

    # `dc_channels` list index (0-based) maps 1:1 to the hub's pv_*_sensor_[]
    # array slot -- pv0 -> index 0, ..., pv3 -> index 3. The channel's own
    # label (e.g. "pv0") is only used for YAML readability/uniqueness
    # checking above; it isn't passed to the hub.
    for i, entry in enumerate(config.get(CONF_DC_CHANNELS, [])):
        _label, channel = next(iter(entry.items()))
        if CONF_VOLTAGE in channel:
            s = await sensor.new_sensor(channel[CONF_VOLTAGE])
            cg.add(hub.set_dc_voltage_sensor(i, s))
        if CONF_CURRENT in channel:
            s = await sensor.new_sensor(channel[CONF_CURRENT])
            cg.add(hub.set_dc_current_sensor(i, s))
        if CONF_POWER in channel:
            s = await sensor.new_sensor(channel[CONF_POWER])
            cg.add(hub.set_dc_power_sensor(i, s))

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
        if CONF_ENERGY_TODAY in ac:
            s = await sensor.new_sensor(ac[CONF_ENERGY_TODAY])
            cg.add(hub.set_ac_energy_today_sensor(s))
        if CONF_ENERGY_TOTAL in ac:
            s = await sensor.new_sensor(ac[CONF_ENERGY_TOTAL])
            cg.add(hub.set_ac_energy_total_sensor(s))

    if CONF_INVERTER in config:
        inv = config[CONF_INVERTER]
        if CONF_TEMPERATURE in inv:
            s = await sensor.new_sensor(inv[CONF_TEMPERATURE])
            cg.add(hub.set_temperature_sensor(s))
        if CONF_RATED_POWER in inv:
            s = await sensor.new_sensor(inv[CONF_RATED_POWER])
            cg.add(hub.set_rated_power_sensor(s))
