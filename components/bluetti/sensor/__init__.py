import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE

from .. import (
    BLUETTI_PLATFORM_SCHEMA,
    CONF_BLUETTI_ID,
    CONF_MODEL,
    bluetti_ns,
    build_tree,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    is_slow,
    tree_schema,
    union_specs,
    walk_config,
)
from ..registers import REGISTERS

DEPENDENCIES = ["bluetti"]

BluettiSensor = bluetti_ns.class_("BluettiSensor", sensor.Sensor, cg.Component)
SensorKind = bluetti_ns.enum("SensorKind", True)

KINDS = {
    "value": SensorKind.VALUE,
    "nibble": SensorKind.NIBBLE,
    "ref_current": SensorKind.REF_CURRENT,
}

# Structure of the YAML (see registers.py): dc_channels: (power, energy, pv0..pv3: power / voltage / current),
# ac: (output, grid, grid_phases / output_phases / inverters maps), battery: (whole system, packs: pack0: ...), and the
# device level entities flat at the root. Maps start at 0.
SPECS = union_specs("sensor")
TREE = build_tree(SPECS)


def _schema(key):
    return sensor.sensor_schema(
        BluettiSensor,
        **entity_kwargs(
            SPECS[key],
            "unit_of_measurement",
            "icon",
            "entity_category",
            "device_class",
            "state_class",
            "accuracy_decimals",
        ),
    )


CONFIG_SCHEMA = BLUETTI_PLATFORM_SCHEMA.extend(tree_schema(TREE, _schema))
FINAL_VALIDATE_SCHEMA = final_validate_platform("sensor", TREE, SPECS)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BLUETTI_ID])
    hub_conf = hub_config(config[CONF_BLUETTI_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["specs"]["sensor"]

    for key, _path, conf in walk_config(TREE, config):
        if key not in available:
            continue
        spec = available[key]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)

        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                MODBUS_REGISTER_TYPE["holding"],
            )
        )
        cg.add(var.set_kind(KINDS[spec["kind"]]))
        cg.add(var.set_scale(spec.get("scale", 1)))
        if "offset" in spec:
            cg.add(var.set_offset(spec["offset"]))
        if "shift" in spec:
            cg.add(var.set_shift(spec["shift"]))
        if "reference" in spec:
            cg.add(var.set_reference(spec["reference"]))
        cg.add(hub.add_item(var, is_slow(spec)))
