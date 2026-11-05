"""Sensor platform (shared by deyelvhy_fast and deyelvhy_slow)."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from esphome.components.deyelvhy import (
    CONF_DEYELVHY_ID,
    DEYELVHY_PLATFORM_SCHEMA,
    add_register_item,
    deyelvhy_ns,
    entity_kwargs,
    final_validate_platform,
    hub_phase_specs,
    transform_args,
    union_specs,
    use_ratio,
    vtype_of,
)

DeyeLvHySensor = deyelvhy_ns.class_("DeyeLvHySensor", sensor.Sensor, cg.Component)
DeyeLvHyCalcSensor = deyelvhy_ns.class_("DeyeLvHyCalcSensor", sensor.Sensor, cg.Component)
CalcOp = deyelvhy_ns.enum("CalcOp", True)

CALC_OPS = {
    "linear": CalcOp.LINEAR,
    "charge_current": CalcOp.CHARGE_CURRENT,
    "discharge_current": CalcOp.DISCHARGE_CURRENT,
    "charge_power": CalcOp.CHARGE_POWER,
    "discharge_power": CalcOp.DISCHARGE_POWER,
}

SPECS = union_specs("sensor")


def _schema(spec):
    cls = DeyeLvHyCalcSensor if spec.get("kind") == "calc" else DeyeLvHySensor
    return sensor.sensor_schema(
        cls,
        **entity_kwargs(
            spec,
            "unit_of_measurement",
            "icon",
            "entity_category",
            "device_class",
            "state_class",
            "accuracy_decimals",
        ),
    )


ENTITY_SCHEMA = DEYELVHY_PLATFORM_SCHEMA.extend({cv.Optional(key): _schema(spec) for key, spec in SPECS.items()})
ENTITY_FINAL_VALIDATE_SCHEMA = final_validate_platform("sensor")


# `platform: deyelvhy` is never written in a YAML: the speed platforms (deyelvhy_fast / deyelvhy_slow) auto-load it so
# that the C++ of this platform is built once, whichever speed(s) are used. It carries no entity itself.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass


async def build_entities(config, slow):
    hub = await cg.get_variable(config[CONF_DEYELVHY_ID])
    specs, factor = hub_phase_specs(config[CONF_DEYELVHY_ID], "sensor")

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)

        if spec.get("kind") == "calc":
            # Computed from registers it reads itself: the source sensors do not have to be declared
            out_scale = -1.0 if spec.get("negate") else 1.0
            cg.add(var.set_op(CALC_OPS[spec["op"]], out_scale))
            coefs = spec.get("coefs", [1] * len(spec["inputs"]))
            for index, inp in enumerate(spec["inputs"]):
                add, scale, wrap = transform_args(inp, factor)
                cg.add(
                    var.add_input(
                        inp["register"],
                        vtype_of(inp),
                        add,
                        scale,
                        wrap,
                        bool(inp.get("bridge")),
                        coefs[index],
                    )
                )
                if inp.get("factor") and factor == "auto":
                    cg.add(var.set_input_ratio(index, hub))
                    cg.add(hub.need_ratio())
                cg.add(hub.add_item(var.get_input(index), slow))
            continue

        add, scale, wrap = transform_args(spec, factor)
        cg.add(var.set_transform(add, scale, wrap))
        use_ratio(hub, var, spec, factor)
        add_register_item(hub, var, spec, slow)
