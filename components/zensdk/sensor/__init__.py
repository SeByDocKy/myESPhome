import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MINUTE,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
)

from .. import CONF_ZENSDK_ID, MAX_PACKS, ZenSdkComponent

DEPENDENCIES = ["zensdk"]

# Conversion ids MUST match the SensorConv enum in zensdk.h.
CONV_LINEAR, CONV_DECIKELVIN, CONV_INT16, CONV_VOLT_AUTO, CONV_TEMP_AUTO, CONV_CELL_DELTA = range(6)

# Icon / device_class / state_class conventions match this author's other components
# (hms, tsungen3, pcm3k6w): "mdi:power", "mdi:sine-wave" (voltage), "mdi:current-dc",
# "mdi:thermometer", "mdi:battery-arrow-up/down", "mdi:numeric" (raw status codes), ...
# Power values are integers on this device, hence accuracy_decimals=0.

# Structure matches this author's hms/hmsw/deyemi components: a `dc_channels` list
# (one entry per MPPT string, 0-indexed -- pv0, pv1, ...), a flat `ac:` block, and,
# specific to this battery component, a `battery:` block (flat fields + a `packs:`
# list, 0-indexed -- pack0, pack1, ...) instead of the former flat pv_power_1..6 /
# pack1_*..pack6_* keys. See README.md "Breaking change" note.
CONF_DC_CHANNELS = "dc_channels"
CONF_AC = "ac"
CONF_BATTERY = "battery"
CONF_PACKS = "packs"
CONF_POWER = "power"


def _power(icon="mdi:power"):
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon=icon,
    )


def _voltage(accuracy=2, icon="mdi:sine-wave"):
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=accuracy,
        icon=icon,
    )


def _dc_current():
    return sensor.sensor_schema(
        unit_of_measurement="A",
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=1,
        icon="mdi:current-dc",
    )


def _temperature():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=1,
        icon="mdi:thermometer",
    )


def _soc(icon="mdi:battery"):
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon=icon,
    )


def _duration():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_MINUTE,
        device_class=DEVICE_CLASS_DURATION,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon="mdi:timer-outline",
    )


def _code(icon="mdi:numeric"):
    """Raw status / mode code, shown as a diagnostic."""
    return sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=icon,
    )


# Device-level entities with no obvious dc_channels/ac/battery home (aggregate PV
# power, radio/enclosure diagnostics): stay flat at the root, like hmsw's
# rssi/warning_number/link_status.
# (config key, zenSDK property, schema, scale, offset, conversion)
ROOT_SENSORS = [
    ("solar_input_power", "solarInputPower", _power("mdi:solar-panel"), 1.0, 0.0, CONV_LINEAR),
    (
        "rssi",
        "rssi",
        sensor.sensor_schema(
            unit_of_measurement="dBm",
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:wifi",
        ),
        1.0,
        0.0,
        CONV_LINEAR,
    ),
    ("enclosure_temperature", "hyperTmp", _temperature(), 1.0, 0.0, CONV_TEMP_AUTO),
    ("fault_level", "faultLevel", _code("mdi:alert-circle-outline"), 1.0, 0.0, CONV_LINEAR),
]

# `dc_channels:` -- one entry per MPPT string. zenSDK only exposes a power value
# per channel (no per-channel voltage/current), unlike deyemi/hmsw.
DC_CHANNEL_SCHEMA = cv.Schema({cv.Optional(CONF_POWER): _power("mdi:solar-panel")})

# `ac:` -- production/home/grid side.
# (config key, zenSDK property, schema, scale, offset, conversion)
AC_FIELDS = [
    ("home_power", "outputHomePower", _power("mdi:transmission-tower-export"), 1.0, 0.0, CONV_LINEAR),
    ("grid_power", "gridInputPower", _power("mdi:transmission-tower-import"), 1.0, 0.0, CONV_LINEAR),
    ("offgrid_power", "gridOffPower", _power("mdi:power-plug-off"), 1.0, 0.0, CONV_LINEAR),
    # acStatus: 0 stopped, 1 grid-tied/off-grid running, 2 charging (docs/zh_properties.md).
    ("status", "acStatus", _code(), 1.0, 0.0, CONV_LINEAR),
]
AC_SCHEMA = cv.Schema({cv.Optional(key): schema for key, _prop, schema, _scale, _offset, _conv in AC_FIELDS})

# `battery:` -- global pack-bus fields...
# (config key, zenSDK property, schema, scale, offset, conversion)
BATTERY_FIELDS = [
    ("soc", "electricLevel", _soc(), 1.0, 0.0, CONV_LINEAR),
    ("voltage", "BatVolt", _voltage(), 0.01, 0.0, CONV_LINEAR),
    ("charge_power", "packInputPower", _power("mdi:battery-arrow-down"), 1.0, 0.0, CONV_LINEAR),
    ("discharge_power", "outputPackPower", _power("mdi:battery-arrow-up"), 1.0, 0.0, CONV_LINEAR),
    ("soc_limit", "socLimit", _code(), 1.0, 0.0, CONV_LINEAR),
    ("charge_max_limit", "chargeMaxLimit", _power("mdi:battery-arrow-up"), 1.0, 0.0, CONV_LINEAR),
    ("pack_count", "packNum", _code("mdi:battery-multiple"), 1.0, 0.0, CONV_LINEAR),
    ("remain_out_time", "remainOutTime", _duration(), 1.0, 0.0, CONV_LINEAR),
    ("remain_input_time", "remainInputTime", _duration(), 1.0, 0.0, CONV_LINEAR),
    # dcStatus: 0 stopped, 1 battery input (charging), 2 battery output (discharging)
    # (docs/zh_properties.md) -- this is the battery/DC-bus state, not the PV side.
    ("status", "dcStatus", _code(), 1.0, 0.0, CONV_LINEAR),
]

# ... and `battery.packs:` -- one entry per battery pack, matched by list position
# to "packData".
# (config key, zenSDK property, schema, scale, offset, conversion)
PACK_FIELDS = [
    ("soc_level", "socLevel", _soc(), 1.0, 0.0, CONV_LINEAR),
    ("power", "power", _power("mdi:battery-arrow-up"), 1.0, 0.0, CONV_LINEAR),
    ("temperature", "maxTemp", _temperature(), 1.0, 0.0, CONV_DECIKELVIN),
    ("total_voltage", "totalVol", _voltage(), 1.0, 0.0, CONV_VOLT_AUTO),
    ("current", "batcur", _dc_current(), 0.1, 0.0, CONV_INT16),
    ("max_cell_voltage", "maxVol", _voltage(accuracy=3), 0.01, 0.0, CONV_LINEAR),
    ("min_cell_voltage", "minVol", _voltage(accuracy=3), 0.01, 0.0, CONV_LINEAR),
    # maxVol - minVol, computed by the hub (property "maxVol" is used for the presence check).
    ("delta_cell_voltage", "maxVol", _voltage(accuracy=3, icon="mdi:delta"), 0.01, 0.0, CONV_CELL_DELTA),
]
PACK_SCHEMA = cv.Schema({cv.Optional(key): schema for key, _prop, schema, _scale, _offset, _conv in PACK_FIELDS})


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
    cv.Length(min=1, max=6)(value)
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


def _pack_entry(value):
    value = cv.Schema({cv.string: PACK_SCHEMA})(value)
    if len(value) != 1:
        raise cv.Invalid(
            "Each 'battery.packs' entry must contain exactly one pack name "
            "(e.g. 'pack0: {soc_level: {name: ...}}')."
        )
    return value


def _validate_packs_list(value):
    value = cv.ensure_list(_pack_entry)(value)
    cv.Length(min=1, max=MAX_PACKS)(value)
    seen = set()
    for entry in value:
        label = next(iter(entry))
        if label in seen:
            raise cv.Invalid(
                f"Pack name '{label}' is used more than once in "
                f"'battery.packs' -- each pack needs a unique name (pack0, pack1, ...)."
            )
        seen.add(label)
    return value


BATTERY_SCHEMA = cv.Schema(
    {
        **{cv.Optional(key): schema for key, _prop, schema, _scale, _offset, _conv in BATTERY_FIELDS},
        cv.Optional(CONF_PACKS): _validate_packs_list,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{cv.Optional(key): schema for key, _prop, schema, _scale, _offset, _conv in ROOT_SENSORS},
        cv.Optional(CONF_DC_CHANNELS): _validate_dc_channels_list,
        cv.Optional(CONF_AC): AC_SCHEMA,
        cv.Optional(CONF_BATTERY): BATTERY_SCHEMA,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])

    for key, prop, _schema, scale, offset, conv in ROOT_SENSORS:
        if key in config:
            s = await sensor.new_sensor(config[key])
            cg.add(hub.add_sensor(prop, scale, offset, conv, -1, s))

    # dc_channels: list position (0-based) -> zenSDK property "solarPower<position+1>".
    # The channel's own label (e.g. "pv0") is only used for YAML readability/uniqueness
    # checking above; it isn't passed to the hub.
    for i, entry in enumerate(config.get(CONF_DC_CHANNELS, [])):
        _label, channel = next(iter(entry.items()))
        if CONF_POWER in channel:
            s = await sensor.new_sensor(channel[CONF_POWER])
            cg.add(hub.add_sensor(f"solarPower{i + 1}", 1.0, 0.0, CONV_LINEAR, -1, s))

    if CONF_AC in config:
        ac = config[CONF_AC]
        for key, prop, _schema, scale, offset, conv in AC_FIELDS:
            if key in ac:
                s = await sensor.new_sensor(ac[key])
                cg.add(hub.add_sensor(prop, scale, offset, conv, -1, s))

    if CONF_BATTERY in config:
        battery = config[CONF_BATTERY]
        for key, prop, _schema, scale, offset, conv in BATTERY_FIELDS:
            if key in battery:
                s = await sensor.new_sensor(battery[key])
                cg.add(hub.add_sensor(prop, scale, offset, conv, -1, s))

        # battery.packs: list position (0-based) -> pack index in "packData", same
        # indexing the hub already used internally before this restructuring.
        for i, entry in enumerate(battery.get(CONF_PACKS, [])):
            _label, pack = next(iter(entry.items()))
            for key, prop, _schema, scale, offset, conv in PACK_FIELDS:
                if key in pack:
                    s = await sensor.new_sensor(pack[key])
                    cg.add(hub.add_sensor(prop, scale, offset, conv, i, s))
