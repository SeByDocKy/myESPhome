"""Sensor platform (shared by deyehylv_fast and deyehylv_slow)."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from esphome.components.deyehylv import (
    CONF_DEYEHYLV_ID,
    DEYEHYLV_PLATFORM_SCHEMA,
    add_register_item,
    deyehylv_ns,
    entity_kwargs,
    final_validate_platform,
    hub_phase_specs,
    transform_args,
    union_specs,
    vtype_of,
)

DeyeHyLvSensor = deyehylv_ns.class_("DeyeHyLvSensor", sensor.Sensor, cg.Component)
DeyeHyLvCalcSensor = deyehylv_ns.class_("DeyeHyLvCalcSensor", sensor.Sensor, cg.Component)
CalcOp = deyehylv_ns.enum("CalcOp", True)

CALC_OPS = {
    "linear": CalcOp.LINEAR,
    "charge_current": CalcOp.CHARGE_CURRENT,
    "discharge_current": CalcOp.DISCHARGE_CURRENT,
    "charge_power": CalcOp.CHARGE_POWER,
    "discharge_power": CalcOp.DISCHARGE_POWER,
}

SPECS = union_specs("sensor")


def _schema(spec):
    cls = DeyeHyLvCalcSensor if spec.get("kind") == "calc" else DeyeHyLvSensor
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


ENTITY_SCHEMA = DEYEHYLV_PLATFORM_SCHEMA.extend({cv.Optional(key): _schema(spec) for key, spec in SPECS.items()})
ENTITY_FINAL_VALIDATE_SCHEMA = final_validate_platform("sensor")


# `platform: deyehylv` is never written in a YAML: the speed platforms (deyehylv_fast / deyehylv_slow) auto-load it so
# that the C++ of this platform is built once, whichever speed(s) are used. It carries no entity itself.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass


async def build_entities(config, slow):
    hub = await cg.get_variable(config[CONF_DEYEHYLV_ID])
    specs, factor = hub_phase_specs(config[CONF_DEYEHYLV_ID], "sensor")

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)

        if spec.get("kind") == "calc":
            # Computed from registers it reads itself: the source sensors do not have to be declared
            out_scale = (factor if spec.get("factor") else 1.0) * (-1.0 if spec.get("negate") else 1.0)
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
                cg.add(hub.add_item(var.get_input(index), slow))
            continue

        add, scale, wrap = transform_args(spec, factor)
        cg.add(var.set_transform(add, scale, wrap))
        add_register_item(hub, var, spec, slow)
