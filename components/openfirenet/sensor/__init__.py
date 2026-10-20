import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_HOUR,
    UNIT_PERCENT,
)

from .. import CONF_OPENFIRENET_ID, OpenFireNetComponent, openfirenet_ns

DEPENDENCIES = ["openfirenet"]

SensorKey = openfirenet_ns.enum("SensorKey")

# key -> (C++ enum name, schema kwargs). Keep in sync with openfirenet.h.
SENSORS = {
    "room_temperature": (
        "SENSOR_ROOM_TEMPERATURE",
        dict(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:thermometer"),
    ),
    "target_temperature": (
        "SENSOR_TARGET_TEMPERATURE",
        dict(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:thermometer-check"),
    ),
    "heating_power": (
        "SENSOR_HEATING_POWER",
        dict(unit_of_measurement=UNIT_PERCENT, accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT,
             icon="mdi:fire"),
    ),
    "flame_temperature": (
        "SENSOR_FLAME_TEMPERATURE",
        dict(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=0, device_class=DEVICE_CLASS_TEMPERATURE,
             state_class=STATE_CLASS_MEASUREMENT, icon="mdi:fire-circle"),
    ),
    "id_fan_measured": (
        "SENSOR_ID_FAN_MEASURED",
        dict(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT, icon="mdi:fan",
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
    "id_fan_set": (
        "SENSOR_ID_FAN_SET",
        dict(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT, icon="mdi:fan-speed-1",
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
    "board_temperature": (
        "SENSOR_BOARD_TEMPERATURE",
        dict(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE,
             state_class=STATE_CLASS_MEASUREMENT, entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
    "rssi": (
        "SENSOR_RSSI",
        dict(unit_of_measurement=UNIT_DECIBEL_MILLIWATT, accuracy_decimals=0,
             device_class=DEVICE_CLASS_SIGNAL_STRENGTH, state_class=STATE_CLASS_MEASUREMENT,
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
    "pellet_hours": (
        "SENSOR_PELLET_HOURS",
        dict(unit_of_measurement=UNIT_HOUR, accuracy_decimals=0, device_class=DEVICE_CLASS_DURATION,
             state_class=STATE_CLASS_TOTAL_INCREASING, icon="mdi:timer-outline"),
    ),
    "pellets_total": (
        "SENSOR_PELLETS_TOTAL",
        dict(unit_of_measurement="kg", accuracy_decimals=0, state_class=STATE_CLASS_TOTAL_INCREASING,
             icon="mdi:grain"),
    ),
    "service_countdown": (
        "SENSOR_SERVICE_COUNTDOWN",
        dict(accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT, icon="mdi:wrench-clock",
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
    "ignition_count": (
        "SENSOR_IGNITION_COUNT",
        dict(accuracy_decimals=0, state_class=STATE_CLASS_TOTAL_INCREASING, icon="mdi:counter",
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
    "on_off_cycles": (
        "SENSOR_ON_OFF_CYCLES",
        dict(accuracy_decimals=0, state_class=STATE_CLASS_TOTAL_INCREASING, icon="mdi:counter",
             entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
    "error_code": (
        "SENSOR_ERROR_CODE",
        dict(accuracy_decimals=0, icon="mdi:alert-circle-outline", entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENFIRENET_ID): cv.use_id(OpenFireNetComponent),
        **{cv.Optional(key): sensor.sensor_schema(**kwargs) for key, (_, kwargs) in SENSORS.items()},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENFIRENET_ID])
    for key, (enum_name, _) in SENSORS.items():
        if conf := config.get(key):
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_sensor(getattr(SensorKey, enum_name), sens))
