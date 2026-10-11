import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_ENERGY_STORAGE,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

from .. import (
    CONF_INDEVOLT_ID,
    GEN1,
    GEN2,
    MAX_DC_CHANNELS,
    MAX_PACKS,
    IndevoltComponent,
    apply_poll,
    both,
    g1,
    g2,
    hub_generation,
    make_final_validate,
    named_list,
    with_poll,
)

DEPENDENCIES = ["indevolt"]

# Icon / device_class / state_class conventions match this author's other components
# (hms, tsungen3, pcm3k6w, zensdk): "mdi:power", "mdi:sine-wave" (voltage), "mdi:current-dc",
# "mdi:thermometer", "mdi:battery-arrow-up/down", ...
#
# Values are published as sent by the device, like the official Home Assistant integration does
# (the only scaling in it is x0.001 on the Gen1 "cumulative production" counter, point 1505).
#
# Structure matches this author's hms/hmsw/deyemi/zensdk components: a `dc_channels` list (one entry per
# PV input, 0-indexed -- pv0, pv1, ...), a flat `ac:` block, a `battery:` block (flat fields + a `packs:`
# list, 0-indexed -- pack0, pack1, ...), and device-level entities flat at the root.
CONF_DC_CHANNELS = "dc_channels"
CONF_AC = "ac"
CONF_BATTERY = "battery"
CONF_PACKS = "packs"


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


def _current(icon="mdi:current-dc"):
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
        icon=icon,
    )


def _frequency():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        device_class=DEVICE_CLASS_FREQUENCY,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
        icon="mdi:sine-wave",
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


def _energy(icon="mdi:counter", unit=UNIT_KILOWATT_HOURS, accuracy=3):
    return sensor.sensor_schema(
        unit_of_measurement=unit,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        accuracy_decimals=accuracy,
        icon=icon,
    )


# Device-level entities with no obvious dc_channels/ac/battery home stay flat at the root.
# (config key, points per generation, schema, scale)
ROOT_SENSORS = [
    (
        "rated_capacity",
        g2(142),
        sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            device_class=DEVICE_CLASS_ENERGY_STORAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=2,
            icon="mdi:battery-high",
        ),
        1.0,
    ),
    ("dc_output_power", both(1501), _power("mdi:solar-power"), 1.0),
    ("daily_production", both(1502), _energy("mdi:solar-power-variant"), 1.0),
    # Gen1 only. The device reports it in Wh; the official integration publishes kWh (x0.001).
    ("total_production", g1(1505), _energy("mdi:solar-power-variant"), 0.001),
    # Gen1: point 21028, Gen2: point 11016.
    ("meter_power", {GEN1: 21028, GEN2: 11016}, _power("mdi:meter-electric"), 1.0),
    # "Emergency power supply" (Gen1) / backup SOC read-back (Gen2), in %.
    ("emergency_power_supply", both(6105), _soc("mdi:battery-heart-variant"), 1.0),
    ("bypass_power", g2(667), _power("mdi:electric-switch"), 1.0),
    ("bypass_input_energy", g2(11034), _energy("mdi:electric-switch", unit=UNIT_WATT_HOURS, accuracy=0), 1.0),
]

# `dc_channels:` -- one entry per PV input, list position (0-based) i -> points 1664+i (power),
# 1600+i (voltage), 1632+i (current). Gen1 only has the power of the first two inputs.
# (config key, points of channel i, schema)
DC_FIELDS = [
    ("power", lambda i: both(1664 + i) if i < 2 else g2(1664 + i), _power("mdi:solar-panel")),
    ("voltage", lambda i: g2(1600 + i), _voltage()),
    ("current", lambda i: g2(1632 + i), _current()),
]
DC_CHANNEL_SCHEMA = cv.Schema({cv.Optional(key): with_poll(schema) for key, _points, schema in DC_FIELDS})

# `ac:` -- AC side of the power module.
# (config key, points per generation, schema)
AC_FIELDS = [
    ("input_power", both(2101), _power("mdi:transmission-tower-export")),
    ("output_power", both(2108), _power("mdi:transmission-tower-import")),
    ("input_energy", both(2107), _energy("mdi:transmission-tower-export")),
    ("output_energy", g2(2104), _energy("mdi:transmission-tower-import")),
    ("offgrid_output_energy", g2(2105), _energy("mdi:power-plug-off")),
    ("voltage", g2(2600), _voltage(accuracy=1)),
    ("frequency", g2(2612), _frequency()),
]
AC_SCHEMA = cv.Schema({cv.Optional(key): with_poll(schema) for key, _points, schema in AC_FIELDS})

# `battery:` -- global battery fields...
# (config key, points per generation, schema)
BATTERY_FIELDS = [
    ("soc", both(6002), _soc()),
    ("power", both(6000), _power("mdi:battery-charging")),
    ("daily_charge_energy", both(6004), _energy("mdi:battery-arrow-down")),
    ("daily_discharge_energy", both(6005), _energy("mdi:battery-arrow-up")),
    ("total_charge_energy", both(6006), _energy("mdi:battery-arrow-down")),
    ("total_discharge_energy", both(6007), _energy("mdi:battery-arrow-up")),
    # Power-module main board ("MB") battery readings -- the integrated pack of the SolidFlex/PowerFlex 2000.
    ("mb_soc", g2(9000), _soc()),
    ("mb_voltage", g2(9004), _voltage()),
    ("mb_current", g2(9013), _current()),
    ("mb_temperature", g2(9012), _temperature()),
    ("mb_cell1_voltage", g2(9009), _voltage(accuracy=3)),
    ("mb_cell2_voltage", g2(9011), _voltage(accuracy=3)),
]

# ... and `battery.packs:` -- one entry per SFA/PFA battery pack, list position (0-based) i -> pack i+1.
# Per-pack points, indexed by pack position. (config key, points per pack, schema)
PACK_FIELDS = [
    ("soc", [9016, 9035, 9054, 9149, 9202], _soc()),
    ("voltage", [9020, 9039, 9058, 9153, 9206], _voltage()),
    ("current", [19173, 19174, 19175, 19176, 19177], _current()),
    ("temperature", [9030, 9049, 9068, 9163, 9216], _temperature()),
    ("cell1_voltage", [9021, 9040, 9059, 9154, 9219], _voltage(accuracy=3)),
    ("cell2_voltage", [9023, 9042, 9061, 9156, 9222], _voltage(accuracy=3)),
]
PACK_SCHEMA = cv.Schema({cv.Optional(key): with_poll(schema) for key, _points, schema in PACK_FIELDS})

BATTERY_SCHEMA = cv.Schema(
    {
        **{cv.Optional(key): with_poll(schema) for key, _points, schema in BATTERY_FIELDS},
        cv.Optional(CONF_PACKS): named_list(PACK_SCHEMA, MAX_PACKS, "pack", "pack0: {soc: {name: ...}}"),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INDEVOLT_ID): cv.use_id(IndevoltComponent),
        **{cv.Optional(key): with_poll(schema) for key, _points, schema, _scale in ROOT_SENSORS},
        cv.Optional(CONF_DC_CHANNELS): named_list(
            DC_CHANNEL_SCHEMA, MAX_DC_CHANNELS, "channel", "pv0: {power: {name: ...}}"
        ),
        cv.Optional(CONF_AC): AC_SCHEMA,
        cv.Optional(CONF_BATTERY): BATTERY_SCHEMA,
    }
)


def _entries(config):
    """Yield (label, points per generation, entity config, scale) for every configured sensor."""
    for key, points, _schema, scale in ROOT_SENSORS:
        if key in config:
            yield key, points, config[key], scale

    for i, entry in enumerate(config.get(CONF_DC_CHANNELS, [])):
        label, channel = next(iter(entry.items()))
        for key, points_fn, _schema in DC_FIELDS:
            if key in channel:
                yield f"{CONF_DC_CHANNELS}.{label}.{key}", points_fn(i), channel[key], 1.0

    ac = config.get(CONF_AC, {})
    for key, points, _schema in AC_FIELDS:
        if key in ac:
            yield f"{CONF_AC}.{key}", points, ac[key], 1.0

    battery = config.get(CONF_BATTERY, {})
    for key, points, _schema in BATTERY_FIELDS:
        if key in battery:
            yield f"{CONF_BATTERY}.{key}", points, battery[key], 1.0
    for i, entry in enumerate(battery.get(CONF_PACKS, [])):
        label, pack = next(iter(entry.items()))
        for key, point_list, _schema in PACK_FIELDS:
            if key in pack:
                yield f"{CONF_BATTERY}.{CONF_PACKS}.{label}.{key}", g2(point_list[i]), pack[key], 1.0


FINAL_VALIDATE_SCHEMA = make_final_validate(_entries)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_INDEVOLT_ID])
    gen = hub_generation(config)
    for _label, points, conf, scale in _entries(config):
        s = await sensor.new_sensor(conf)
        cg.add(hub.add_sensor(points[gen], scale, 0.0, s))
        apply_poll(hub, points[gen], conf)
