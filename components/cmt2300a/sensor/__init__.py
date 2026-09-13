import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from .. import CMT2300AComponent, cmt2300a_ns

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["cmt2300a"]

CONF_CMT2300A_ID = "cmt2300a_id"
CONF_DUTY_CYCLE = "duty_cycle"
CONF_HMS_COUNT = "hms_count"

CMT2300ADutyCycleSensor = cmt2300a_ns.class_(
    "CMT2300ADutyCycleSensor", sensor.Sensor, cg.PollingComponent
)

_DUTY_CYCLE_SCHEMA = sensor.sensor_schema(
    CMT2300ADutyCycleSensor,
    unit_of_measurement=UNIT_PERCENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:radio-tower",
).extend(cv.polling_component_schema("60s"))

# hms_count is an event-driven counter (published by cmt2300a: on every reachable
# state change of an hms:, not polled) -- a base sensor::Sensor is enough,
# no dedicated C++ class needed like for duty_cycle.
_HMS_COUNT_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    icon="mdi:numeric",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CMT2300A_ID): cv.use_id(CMT2300AComponent),
        cv.Optional(CONF_DUTY_CYCLE): _DUTY_CYCLE_SCHEMA,
        cv.Optional(CONF_HMS_COUNT): _HMS_COUNT_SCHEMA,
    }
)


def _final_validate(config):
    if CONF_HMS_COUNT not in config:
        return config

    # hms_count only makes sense if at least one hms: is attached to this
    # cmt2300a: -- otherwise the counter would always stay at 0, which is more
    # likely a config oversight than actual intent.
    try:
        full_conf = fv.full_config.get()
        hms_confs = full_conf.get("hms", [])
        if isinstance(hms_confs, dict):
            hms_confs = [hms_confs]
    except Exception:  # noqa: BLE001 -- internal API may differ across
        # ESPHome versions; don't block compilation because of that.
        return config

    radio_id = config[CONF_CMT2300A_ID]
    if not any(hms_conf.get("cmt2300a_id") == radio_id for hms_conf in hms_confs):
        raise cv.Invalid(
            f"'hms_count' requires at least one hms: component attached to "
            f"this cmt2300a_id ('{radio_id}') -- without an hms:, this counter would "
            f"always stay at 0."
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    radio = await cg.get_variable(config[CONF_CMT2300A_ID])

    if CONF_DUTY_CYCLE in config:
        conf = config[CONF_DUTY_CYCLE]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(radio))

    if CONF_HMS_COUNT in config:
        s = await sensor.new_sensor(config[CONF_HMS_COUNT])
        cg.add(radio.set_hms_count_sensor(s))
