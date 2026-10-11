import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_PERCENT, UNIT_WATT

from .. import (
    CONF_INDEVOLT_ID,
    MAX_REALTIME_POWER,
    IndevoltComponent,
    apply_poll,
    both,
    g2,
    hub_generation,
    indevolt_ns,
    make_final_validate,
    with_poll,
)

DEPENDENCIES = ["indevolt"]

IndevoltNumber = indevolt_ns.class_("IndevoltNumber", number.Number, cg.Parented.template(IndevoltComponent))

# Kind ids MUST match the NumberKind enum in indevolt.h.
KIND_SETTING, KIND_REALTIME_POWER, KIND_POWER_SETPOINT, KIND_CHARGE_POWER, KIND_DISCHARGE_POWER = range(5)

# Ranges are the widest of both generations; the hub clamps every power request to the limits configured
# on the `indevolt:` block (`max_charge_power` / `max_discharge_power`) and publishes the clamped value back.
# (config key, read points per generation or None, write point, kind, min, max, step, unit, icon, entity_category)
NUMBERS = [
    # Signed: > 0 discharge, < 0 charge, 0 standby. Sends the complete real-time command (point 47015),
    # switching the device to Real-Time Control first.
    ("power_setpoint", None, 0, KIND_POWER_SETPOINT, -MAX_REALTIME_POWER, MAX_REALTIME_POWER, 1, UNIT_WATT, "mdi:swap-vertical", None),
    # Charge / discharge power in W (>= 0), one number per direction (like `input_limit` / `output_limit` of the
    # zensdk component). Each sends the complete real-time command; setting one direction replaces the other, and
    # 0 W only stops the direction it belongs to.
    ("charging_power", None, 0, KIND_CHARGE_POWER, 0, MAX_REALTIME_POWER, 1, UNIT_WATT, "mdi:battery-arrow-up", None),
    ("discharging_power", None, 0, KIND_DISCHARGE_POWER, 0, MAX_REALTIME_POWER, 1, UNIT_WATT, "mdi:battery-arrow-down", None),
    # Backup / emergency SOC (SolidFlex/PowerFlex 2000). Read back from point 6105.
    ("backup_soc", g2(6105), 1142, KIND_SETTING, 5, 100, 1, UNIT_PERCENT, "mdi:battery-heart-variant", ENTITY_CATEGORY_CONFIG),
    ("inverter_input_limit", g2(11009), 1138, KIND_SETTING, 50, 2400, 1, UNIT_WATT, "mdi:battery-arrow-up", ENTITY_CATEGORY_CONFIG),
    ("max_output_power", g2(11011), 1147, KIND_SETTING, 50, 2400, 1, UNIT_WATT, "mdi:battery-arrow-down", ENTITY_CATEGORY_CONFIG),
    ("feed_in_power_limit", g2(11010), 1146, KIND_SETTING, 50, 2400, 1, UNIT_WATT, "mdi:transmission-tower-export", ENTITY_CATEGORY_CONFIG),
    # Raw real-time control settings (points 47016 / 47017), as exposed by the official Home Assistant integration:
    # they change one parameter of the running real-time command. Not readable on the device.
    ("power_setting", None, 47016, KIND_REALTIME_POWER, 0, MAX_REALTIME_POWER, 1, UNIT_WATT, "mdi:flash", ENTITY_CATEGORY_CONFIG),
    ("soc_setting", None, 47017, KIND_SETTING, 0, 100, 1, UNIT_PERCENT, "mdi:percent", ENTITY_CATEGORY_CONFIG),
]
# Entities available on both generations (the others are SolidFlex/PowerFlex 2000 only).
_BOTH = {"power_setpoint", "charging_power", "discharging_power", "power_setting", "soc_setting"}


def _schema(icon, unit, category):
    kwargs = dict(icon=icon, unit_of_measurement=unit)
    if category is not None:
        kwargs["entity_category"] = category
    return with_poll(number.number_schema(IndevoltNumber, **kwargs))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INDEVOLT_ID): cv.use_id(IndevoltComponent),
        **{
            cv.Optional(key): _schema(icon, unit, category)
            for key, _read, _write, _kind, _min, _max, _step, unit, icon, category in NUMBERS
        },
    }
)


def _entries(config):
    for key, read, write, kind, min_v, max_v, step, _unit, _icon, _category in NUMBERS:
        if key in config:
            availability = both(0) if key in _BOTH else g2(0)
            yield key, availability, config[key], (read, write, kind, min_v, max_v, step)


FINAL_VALIDATE_SCHEMA = make_final_validate(_entries)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_INDEVOLT_ID])
    gen = hub_generation(config)
    for _label, _availability, conf, (read, write, kind, min_v, max_v, step) in _entries(config):
        var = await number.new_number(conf, min_value=float(min_v), max_value=float(max_v), step=float(step))
        await cg.register_parented(var, hub)
        cg.add(var.set_kind(kind))
        cg.add(var.set_write_point(write))
        read_point = read[gen] if read is not None else 0
        cg.add(hub.add_number(kind, read_point, write, var))
        apply_poll(hub, read_point, conf)
