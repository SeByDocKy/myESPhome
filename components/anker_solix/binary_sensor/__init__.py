import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE
import esphome.config_validation as cv

from .. import (
    CONF_ANKER_SOLIX_ID,
    CONF_MODEL,
    ANKER_SOLIX_PLATFORM_SCHEMA,
    anker_solix_ns,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    is_slow,
    union_specs,
)
from ..registers import REGISTERS

DEPENDENCIES = ["anker_solix"]

AnkerSolixBinarySensor = anker_solix_ns.class_("AnkerSolixBinarySensor", binary_sensor.BinarySensor, cg.Component)

SPECS = union_specs("binary_sensor")


def _schema(spec):
    # The connection sensor is a plain BinarySensor driven by the hub
    cls = binary_sensor.BinarySensor if spec["kind"] == "connection" else AnkerSolixBinarySensor
    return binary_sensor.binary_sensor_schema(
        cls, **entity_kwargs(spec, "icon", "entity_category", "device_class")
    )


CONFIG_SCHEMA = ANKER_SOLIX_PLATFORM_SCHEMA.extend(
    {cv.Optional(key): _schema(spec) for key, spec in SPECS.items()}
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("binary_sensor")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ANKER_SOLIX_ID])
    hub_conf = hub_config(config[CONF_ANKER_SOLIX_ID])
    specs = REGISTERS[hub_conf[CONF_MODEL]]["binary_sensor"]

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await binary_sensor.new_binary_sensor(conf)

        if spec["kind"] == "connection":
            # No register: the hub reports whether the battery answers
            cg.add(hub.set_connection_sensor(var))
            continue

        await cg.register_component(var, conf)
        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                MODBUS_REGISTER_TYPE[spec["rtype"]],
            )
        )
        cg.add(hub.add_item(var, is_slow(spec)))
