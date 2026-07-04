import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DEGREES,
    UNIT_EMPTY,
)

from . import BNO055Component, CONF_BNO055_ID

DEPENDENCIES = ["bno055"]

CONF_HEADING = "heading"
CONF_ROLL = "roll"
CONF_PITCH = "pitch"

CONF_QUATERNION_W = "quaternion_w"
CONF_QUATERNION_X = "quaternion_x"
CONF_QUATERNION_Y = "quaternion_y"
CONF_QUATERNION_Z = "quaternion_z"

CONF_LINEAR_ACCELERATION_X = "linear_acceleration_x"
CONF_LINEAR_ACCELERATION_Y = "linear_acceleration_y"
CONF_LINEAR_ACCELERATION_Z = "linear_acceleration_z"

CONF_GRAVITY_X = "gravity_x"
CONF_GRAVITY_Y = "gravity_y"
CONF_GRAVITY_Z = "gravity_z"

CONF_ACCELERATION_X = "acceleration_x"
CONF_ACCELERATION_Y = "acceleration_y"
CONF_ACCELERATION_Z = "acceleration_z"

CONF_GYROSCOPE_X = "gyroscope_x"
CONF_GYROSCOPE_Y = "gyroscope_y"
CONF_GYROSCOPE_Z = "gyroscope_z"

CONF_MAGNETIC_FIELD_X = "magnetic_field_x"
CONF_MAGNETIC_FIELD_Y = "magnetic_field_y"
CONF_MAGNETIC_FIELD_Z = "magnetic_field_z"

CONF_CALIBRATION_SYSTEM = "calibration_system"
CONF_CALIBRATION_GYROSCOPE = "calibration_gyroscope"
CONF_CALIBRATION_ACCELEROMETER = "calibration_accelerometer"
CONF_CALIBRATION_MAGNETOMETER = "calibration_magnetometer"

UNIT_METER_PER_SECOND_SQUARED = "m/s²"
UNIT_DEGREE_PER_SECOND = "°/s"
UNIT_MICROTESLA = "µT"

ICON_AXIS_ARROW = "mdi:axis-arrow"
ICON_COMPASS = "mdi:compass"
ICON_ORBIT = "mdi:orbit-variant"
ICON_CALIBRATION = "mdi:check-decagram-outline"

# All the fields this platform can expose, mapped to the C++ setter suffix
# used by the SUB_SENSOR(name) macros in bno055.h (`set_<key>_sensor`).
_ACCEL_KEYS = (
    CONF_LINEAR_ACCELERATION_X,
    CONF_LINEAR_ACCELERATION_Y,
    CONF_LINEAR_ACCELERATION_Z,
    CONF_GRAVITY_X,
    CONF_GRAVITY_Y,
    CONF_GRAVITY_Z,
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
)
_GYRO_KEYS = (CONF_GYROSCOPE_X, CONF_GYROSCOPE_Y, CONF_GYROSCOPE_Z)
_MAG_KEYS = (CONF_MAGNETIC_FIELD_X, CONF_MAGNETIC_FIELD_Y, CONF_MAGNETIC_FIELD_Z)
_ANGLE_KEYS = (CONF_HEADING, CONF_ROLL, CONF_PITCH)
_QUATERNION_KEYS = (CONF_QUATERNION_W, CONF_QUATERNION_X, CONF_QUATERNION_Y, CONF_QUATERNION_Z)
_CALIBRATION_KEYS = (
    CONF_CALIBRATION_SYSTEM,
    CONF_CALIBRATION_GYROSCOPE,
    CONF_CALIBRATION_ACCELEROMETER,
    CONF_CALIBRATION_MAGNETOMETER,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BNO055_ID): cv.use_id(BNO055Component),
        **{
            cv.Optional(key): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon=ICON_COMPASS if key == CONF_HEADING else ICON_AXIS_ARROW,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            for key in _ANGLE_KEYS
        },
        **{
            cv.Optional(key): sensor.sensor_schema(
                icon=ICON_ORBIT,
                accuracy_decimals=4,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            for key in _QUATERNION_KEYS
        },
        **{
            cv.Optional(key): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
                icon=ICON_AXIS_ARROW,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            for key in _ACCEL_KEYS
        },
        **{
            cv.Optional(key): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREE_PER_SECOND,
                icon=ICON_AXIS_ARROW,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            for key in _GYRO_KEYS
        },
        **{
            cv.Optional(key): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROTESLA,
                icon=ICON_AXIS_ARROW,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            for key in _MAG_KEYS
        },
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        **{
            cv.Optional(key): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_CALIBRATION,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            for key in _CALIBRATION_KEYS
        },
    }
)

_ALL_KEYS = (
    _ANGLE_KEYS + _QUATERNION_KEYS + _ACCEL_KEYS + _GYRO_KEYS + _MAG_KEYS
    + (CONF_TEMPERATURE,) + _CALIBRATION_KEYS
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BNO055_ID])

    for key in _ALL_KEYS:
        if key not in config:
            continue
        sens = await sensor.new_sensor(config[key])
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))
