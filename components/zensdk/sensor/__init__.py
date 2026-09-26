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
CONV_LINEAR, CONV_DECIKELVIN, CONV_INT16, CONV_VOLT_AUTO, CONV_TEMP_AUTO = range(5)

# Icon / device_class / state_class conventions match this author's other components
# (hms, tsungen3, pcm3k6w): "mdi:power", "mdi:sine-wave" (voltage), "mdi:current-dc",
# "mdi:thermometer", "mdi:battery-arrow-up/down", "mdi:numeric" (raw status codes), ...
# Power values are integers on this device, hence accuracy_decimals=0.


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


# (config key, zenSDK property, schema, scale, offset, conversion, pack index or -1)
SENSORS = [
    ("electric_level", "electricLevel", _soc(), 1.0, 0.0, CONV_LINEAR, -1),
    ("solar_input_power", "solarInputPower", _power("mdi:solar-panel"), 1.0, 0.0, CONV_LINEAR, -1),
    ("pack_input_power", "packInputPower", _power("mdi:battery-arrow-down"), 1.0, 0.0, CONV_LINEAR, -1),
    ("output_pack_power", "outputPackPower", _power("mdi:battery-arrow-up"), 1.0, 0.0, CONV_LINEAR, -1),
    ("output_home_power", "outputHomePower", _power("mdi:transmission-tower-export"), 1.0, 0.0, CONV_LINEAR, -1),
    ("grid_input_power", "gridInputPower", _power("mdi:transmission-tower-import"), 1.0, 0.0, CONV_LINEAR, -1),
    ("grid_off_power", "gridOffPower", _power("mdi:power-plug-off"), 1.0, 0.0, CONV_LINEAR, -1),
    ("battery_voltage", "BatVolt", _voltage(), 0.01, 0.0, CONV_LINEAR, -1),
    ("enclosure_temperature", "hyperTmp", _temperature(), 1.0, 0.0, CONV_TEMP_AUTO, -1),
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
        -1,
    ),
    ("remain_out_time", "remainOutTime", _duration(), 1.0, 0.0, CONV_LINEAR, -1),
    ("remain_input_time", "remainInputTime", _duration(), 1.0, 0.0, CONV_LINEAR, -1),
    ("charge_max_limit", "chargeMaxLimit", _power("mdi:battery-arrow-up"), 1.0, 0.0, CONV_LINEAR, -1),
    ("pack_num", "packNum", _code("mdi:battery-multiple"), 1.0, 0.0, CONV_LINEAR, -1),
    ("soc_limit", "socLimit", _code(), 1.0, 0.0, CONV_LINEAR, -1),
    ("fault_level", "faultLevel", _code("mdi:alert-circle-outline"), 1.0, 0.0, CONV_LINEAR, -1),
    ("dc_status", "dcStatus", _code(), 1.0, 0.0, CONV_LINEAR, -1),
    ("ac_status", "acStatus", _code(), 1.0, 0.0, CONV_LINEAR, -1),
]

# PV channels 1..6
for _n in range(1, 7):
    SENSORS.append(
        (f"solar_power_{_n}", f"solarPower{_n}", _power("mdi:solar-panel"), 1.0, 0.0, CONV_LINEAR, -1)
    )

# Per-battery-pack entities, matched by position in "packData".
for _n in range(1, MAX_PACKS + 1):
    _p = _n - 1
    SENSORS += [
        (f"pack{_n}_soc_level", "socLevel", _soc(), 1.0, 0.0, CONV_LINEAR, _p),
        (f"pack{_n}_power", "power", _power("mdi:battery-arrow-up"), 1.0, 0.0, CONV_LINEAR, _p),
        (f"pack{_n}_temperature", "maxTemp", _temperature(), 1.0, 0.0, CONV_DECIKELVIN, _p),
        (f"pack{_n}_total_voltage", "totalVol", _voltage(), 1.0, 0.0, CONV_VOLT_AUTO, _p),
        (f"pack{_n}_current", "batcur", _dc_current(), 0.1, 0.0, CONV_INT16, _p),
        (f"pack{_n}_max_cell_voltage", "maxVol", _voltage(accuracy=3), 0.01, 0.0, CONV_LINEAR, _p),
        (f"pack{_n}_min_cell_voltage", "minVol", _voltage(accuracy=3), 0.01, 0.0, CONV_LINEAR, _p),
    ]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{cv.Optional(key): schema for key, _prop, schema, _scale, _offset, _conv, _pack in SENSORS},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])
    for key, prop, _schema, scale, offset, conv, pack in SENSORS:
        if key in config:
            s = await sensor.new_sensor(config[key])
            cg.add(hub.add_sensor(prop, scale, offset, conv, pack, s))
