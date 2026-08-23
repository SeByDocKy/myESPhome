import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_MODE, UNIT_AMPERE, UNIT_PERCENT, UNIT_VOLT

from .. import CONF_PCM3K6W_ID, PCM3K6W_PLATFORM_SCHEMA, pcm3k6w_ns

DEPENDENCIES = ["canbus"]

PCM3K6WNumber = pcm3k6w_ns.class_("PCM3K6WNumber", number.Number, cg.Component)

# Kind indices MUST match the NumberKind enum order in pcm3k6w.h.
(
    KIND_CHARGING_VOLTAGE,
    KIND_CHARGING_CURRENT,
    KIND_DISCHARGING_CURRENT,
    KIND_AC_OVERVOLTAGE_PROTECTION,
    KIND_AC_UNDERVOLTAGE_PROTECTION,
    KIND_AC_OVERVOLTAGE_ALARM,
    KIND_AC_UNDERVOLTAGE_ALARM,
    KIND_DC_OVERVOLTAGE_PROTECTION,
    KIND_DC_UNDERVOLTAGE_PROTECTION,
    KIND_DC_OVERVOLTAGE_ALARM,
    KIND_DC_UNDERVOLTAGE_ALARM,
    KIND_FAN_SPEED,
) = range(12)

# (config key, kind, unit, icon, min, max, step, initial_value)
NUMBERS = [
    ("charging_voltage", KIND_CHARGING_VOLTAGE, UNIT_VOLT, "mdi:battery-charging-high", 40.0, 59.0, 0.1, 56.0),
    ("charging_current", KIND_CHARGING_CURRENT, UNIT_AMPERE, "mdi:battery-arrow-up", 1.0, 70.0, 0.5, 1.0),
    ("discharging_current", KIND_DISCHARGING_CURRENT, UNIT_AMPERE, "mdi:battery-arrow-down", 1.0, 75.0, 0.5, 1.0),
    ("ac_overvoltage_protection", KIND_AC_OVERVOLTAGE_PROTECTION, UNIT_VOLT, "mdi:flash-alert", 220.0, 258.0, 0.5, 250.0),
    ("ac_undervoltage_protection", KIND_AC_UNDERVOLTAGE_PROTECTION, UNIT_VOLT, "mdi:flash-alert-outline", 182.0, 220.0, 0.5, 200.0),
    ("ac_overvoltage_alarm", KIND_AC_OVERVOLTAGE_ALARM, UNIT_VOLT, "mdi:flash-alert", 220.0, 258.0, 0.5, 248.0),
    ("ac_undervoltage_alarm", KIND_AC_UNDERVOLTAGE_ALARM, UNIT_VOLT, "mdi:flash-alert-outline", 182.0, 220.0, 0.5, 205.0),
    ("dc_overvoltage_protection", KIND_DC_OVERVOLTAGE_PROTECTION, UNIT_VOLT, "mdi:battery-alert", 40.0, 59.5, 0.1, 58.0),
    ("dc_undervoltage_protection", KIND_DC_UNDERVOLTAGE_PROTECTION, UNIT_VOLT, "mdi:battery-alert-variant-outline", 39.5, 59.0, 0.1, 49.0),
    ("dc_overvoltage_alarm", KIND_DC_OVERVOLTAGE_ALARM, UNIT_VOLT, "mdi:battery-alert", 40.0, 57.5, 0.1, 56.6),
    ("dc_undervoltage_alarm", KIND_DC_UNDERVOLTAGE_ALARM, UNIT_VOLT, "mdi:battery-alert-variant-outline", 40.0, 59.0, 0.1, 50.0),
    ("fan_speed", KIND_FAN_SPEED, UNIT_PERCENT, "mdi:fan", 0.0, 100.0, 1.0, 0.0),
]

CONFIG_SCHEMA = PCM3K6W_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): number.number_schema(
            PCM3K6WNumber,
            unit_of_measurement=unit,
            icon=icon,
        ).extend(
            {cv.Optional(CONF_MODE, default="SLIDER"): cv.enum(number.NUMBER_MODES, upper=True)}
        )
        for key, _kind, unit, icon, _min, _max, _step, _init in NUMBERS
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PCM3K6W_ID])
    for key, kind, _unit, _icon, min_value, max_value, step, initial_value in NUMBERS:
        if key in config:
            n = await number.new_number(
                config[key],
                min_value=min_value,
                max_value=max_value,
                step=step,
            )
            await cg.register_component(n, config[key])
            await cg.register_parented(n, hub)
            cg.add(n.set_initial_value(initial_value))
            cg.add(n.set_kind(kind))
            cg.add(hub.set_number(kind, n))
