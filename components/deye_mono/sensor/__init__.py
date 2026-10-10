import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from .. import (
    CONF_DEYE_MONO_ID,
    CONF_INVERTER_FACTOR,
    DEYE_MONO_PLATFORM_SCHEMA,
    add_register_item,
    deye_mono_ns,
    entity_kwargs,
    hub_config,
    transform_args,
    vtype_of,
)
from ..registers import SENSORS

DEPENDENCIES = ["deye_mono"]

DeyeMonoSensor = deye_mono_ns.class_("DeyeMonoSensor", sensor.Sensor, cg.Component)
DeyeMonoCalcSensor = deye_mono_ns.class_("DeyeMonoCalcSensor", sensor.Sensor, cg.Component)
CalcOp = deye_mono_ns.enum("CalcOp", True)

CALC_OPS = {
    "linear": CalcOp.LINEAR,
    "charge_current": CalcOp.CHARGE_CURRENT,
    "discharge_current": CalcOp.DISCHARGE_CURRENT,
    "charge_power": CalcOp.CHARGE_POWER,
    "discharge_power": CalcOp.DISCHARGE_POWER,
}


def _schema(spec):
    cls = DeyeMonoCalcSensor if spec.get("kind") == "calc" else DeyeMonoSensor
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


CONFIG_SCHEMA = DEYE_MONO_PLATFORM_SCHEMA.extend(
    {cv.Optional(key): _schema(spec) for key, spec in SENSORS.items()}
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYE_MONO_ID])
    factor = hub_config(config[CONF_DEYE_MONO_ID])[CONF_INVERTER_FACTOR]

    for key, conf in config.items():
        if key not in SENSORS:
            continue
        spec = SENSORS[key]
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
                cg.add(hub.add_item(var.get_input(index)))
            continue

        add, scale, wrap = transform_args(spec, factor)
        cg.add(var.set_transform(add, scale, wrap))
        add_register_item(hub, var, spec)
