import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_WATT,
)

from .. import CONF_MULTIRS_ID, MultiRS, multirs_ns

DEPENDENCIES = ["multirs"]

MultiRSSensor = multirs_ns.class_("MultiRSSensor", sensor.Sensor, cg.Parented.template(MultiRS))


def _s(unit, decimals, device_class, state_class=STATE_CLASS_MEASUREMENT, **extra):
    return dict(
        unit_of_measurement=unit,
        accuracy_decimals=decimals,
        device_class=device_class,
        state_class=state_class,
        **extra,
    )


# key -> (SensorKind value, schema). The numeric kinds mirror the C++ enum SensorKind in multirs.h.
# Scales of the register based ones are ASSUMED (see README): correct them with a `multiply:` filter.
SENSORS = {
    "battery_voltage": (0, _s(UNIT_VOLT, 2, DEVICE_CLASS_VOLTAGE, icon="mdi:battery-charging")),
    "battery_current": (1, _s(UNIT_AMPERE, 1, DEVICE_CLASS_CURRENT, icon="mdi:current-dc")),
    "battery_power": (2, _s(UNIT_WATT, 0, DEVICE_CLASS_POWER, icon="mdi:flash")),
    "battery_temperature": (3, _s(UNIT_CELSIUS, 1, DEVICE_CLASS_TEMPERATURE)),
    "ac_in_voltage": (4, _s(UNIT_VOLT, 1, DEVICE_CLASS_VOLTAGE, icon="mdi:transmission-tower-import")),
    "ac_in_current": (5, _s(UNIT_AMPERE, 1, DEVICE_CLASS_CURRENT, icon="mdi:transmission-tower-import")),
    "ac_in_power": (6, _s(UNIT_WATT, 0, DEVICE_CLASS_POWER, icon="mdi:transmission-tower-import")),
    "ac_in_apparent_power": (7, _s(UNIT_VOLT_AMPS, 0, DEVICE_CLASS_APPARENT_POWER, icon="mdi:transmission-tower-import")),
    "ac_in_frequency": (8, _s(UNIT_HERTZ, 2, DEVICE_CLASS_FREQUENCY, icon="mdi:sine-wave")),
    "ac_out_voltage": (9, _s(UNIT_VOLT, 1, DEVICE_CLASS_VOLTAGE, icon="mdi:transmission-tower-export")),
    "ac_out_current": (10, _s(UNIT_AMPERE, 1, DEVICE_CLASS_CURRENT, icon="mdi:transmission-tower-export")),
    "ac_out_power": (11, _s(UNIT_WATT, 0, DEVICE_CLASS_POWER, icon="mdi:transmission-tower-export")),
    "ac_out_apparent_power": (12, _s(UNIT_VOLT_AMPS, 0, DEVICE_CLASS_APPARENT_POWER, icon="mdi:transmission-tower-export")),
    "ac_out_frequency": (13, _s(UNIT_HERTZ, 2, DEVICE_CLASS_FREQUENCY, icon="mdi:sine-wave")),
    "pv_voltage": (14, _s(UNIT_VOLT, 1, DEVICE_CLASS_VOLTAGE, icon="mdi:solar-panel")),
    "pv_power": (15, _s(UNIT_WATT, 0, DEVICE_CLASS_POWER, icon="mdi:solar-power")),
    "energy_today": (16, _s(UNIT_KILOWATT_HOURS, 2, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING, icon="mdi:solar-power-variant")),
    "energy_yesterday": (17, _s(UNIT_KILOWATT_HOURS, 2, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING, icon="mdi:solar-power-variant-outline")),
    "energy_total": (18, _s(UNIT_KILOWATT_HOURS, 2, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING, icon="mdi:counter")),
    "internal_temperature": (19, _s(UNIT_CELSIUS, 1, DEVICE_CLASS_TEMPERATURE, entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MULTIRS_ID): cv.use_id(MultiRS),
        **{
            cv.Optional(key): sensor.sensor_schema(MultiRSSensor, **kwargs)
            for key, (_, kwargs) in SENSORS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MULTIRS_ID])
    for key, (kind, _) in SENSORS.items():
        if key in config:
            var = await sensor.new_sensor(config[key])
            await cg.register_parented(var, config[CONF_MULTIRS_ID])
            cg.add(var.set_kind(kind))
            cg.add(hub.register_sensor(var))
