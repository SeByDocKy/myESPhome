import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
    UNIT_HERTZ,
    UNIT_CELSIUS,
    UNIT_KILOWATT_HOURS,
    UNIT_SECOND,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_ENERGY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)
from .. import ez1m_ns, EZ1MComponent, CONF_EZ1M_ID

DEPENDENCIES = ["ez1m"]

EZ1MSensor = ez1m_ns.class_("EZ1MSensor", sensor.Sensor, cg.Parented.template(EZ1MComponent))
EZ1MSensorType = ez1m_ns.enum("EZ1MSensorType", is_class=True)

CONF_CH1_DC_VOLTAGE = "ch1_dc_voltage"
CONF_CH2_DC_VOLTAGE = "ch2_dc_voltage"
CONF_CH1_DC_CURRENT = "ch1_dc_current"
CONF_CH2_DC_CURRENT = "ch2_dc_current"
CONF_CH1_DC_POWER = "ch1_dc_power"
CONF_CH2_DC_POWER = "ch2_dc_power"
CONF_TOTAL_DC_POWER = "total_dc_power"
CONF_AC_POWER = "ac_power"
CONF_GRID_FREQUENCY = "grid_frequency"
CONF_TEMPERATURE = "temperature"
CONF_DAILY_ENERGY = "daily_energy"
CONF_CH1_SESSION_ENERGY = "ch1_session_energy"
CONF_CH2_SESSION_ENERGY = "ch2_session_energy"
CONF_LIFETIME_ENERGY = "lifetime_energy"
CONF_INVERTER_UPTIME = "inverter_uptime"

# Icon conventions below are kept in sync with the `hms` component's
# sensor/__init__.py so that similarly-typed entities look the same across
# both components: mdi:power (W), mdi:current-dc (A), mdi:sine-wave (V),
# mdi:metronome (Hz), mdi:thermometer (deg C), mdi:counter (energy).
SENSOR_TYPES = {
    CONF_CH1_DC_VOLTAGE: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:sine-wave",
    ),
    CONF_CH2_DC_VOLTAGE: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:sine-wave",
    ),
    CONF_CH1_DC_CURRENT: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:current-dc",
    ),
    CONF_CH2_DC_CURRENT: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:current-dc",
    ),
    CONF_CH1_DC_POWER: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:power",
    ),
    CONF_CH2_DC_POWER: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:power",
    ),
    CONF_TOTAL_DC_POWER: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:power",
    ),
    CONF_AC_POWER: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:power",
    ),
    CONF_GRID_FREQUENCY: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_HERTZ,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_FREQUENCY,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:metronome",
    ),
    CONF_TEMPERATURE: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:thermometer",
    ),
    CONF_DAILY_ENERGY: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        icon="mdi:counter",
    ),
    CONF_CH1_SESSION_ENERGY: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        icon="mdi:counter",
    ),
    CONF_CH2_SESSION_ENERGY: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        icon="mdi:counter",
    ),
    CONF_LIFETIME_ENERGY: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        icon="mdi:counter",
    ),
    CONF_INVERTER_UPTIME: sensor.sensor_schema(
        EZ1MSensor,
        unit_of_measurement=UNIT_SECOND,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EZ1M_ID): cv.use_id(EZ1MComponent),
        **{cv.Optional(key): schema for key, schema in SENSOR_TYPES.items()},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EZ1M_ID])
    for key in SENSOR_TYPES:
        if key not in config:
            continue
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        await cg.register_parented(sens, hub)
        cg.add(hub.register_sensor(sens, getattr(EZ1MSensorType, key.upper())))
