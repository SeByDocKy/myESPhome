import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.modbustcp_controller import SENSOR_VALUE_TYPE
import esphome.config_validation as cv

from .. import (
    CONF_MARSTEK_ID,
    CONF_MODEL,
    MARSTEK_PLATFORM_SCHEMA,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    marstek_ns,
    skip_updates,
    union_specs,
)
from ..registers import REGISTERS

DEPENDENCIES = ["marstek"]

MarstekBinarySensor = marstek_ns.class_(
    "MarstekBinarySensor", binary_sensor.BinarySensor, cg.Component
)
BinaryKind = marstek_ns.enum("BinaryKind", True)

KINDS = {"bool": BinaryKind.BOOL, "bits_any": BinaryKind.BITS_ANY}

SPECS = union_specs("binary_sensor")

CONFIG_SCHEMA = MARSTEK_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): binary_sensor.binary_sensor_schema(
            MarstekBinarySensor,
            **entity_kwargs(spec, "icon", "entity_category", "device_class"),
        )
        for key, spec in SPECS.items()
    }
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("binary_sensor")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MARSTEK_ID])
    hub_conf = hub_config(config[CONF_MARSTEK_ID])
    specs = REGISTERS[hub_conf[CONF_MODEL]]["binary_sensor"]

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await binary_sensor.new_binary_sensor(conf)
        await cg.register_component(var, conf)

        if spec["kind"] == "connection":
            # No register: the hub reports whether the battery answers
            cg.add(hub.set_connection_sensor(var))
            continue

        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE["U_WORD"],
                skip_updates(spec, hub_conf),
            )
        )
        cg.add(var.set_kind(KINDS[spec["kind"]]))
        cg.add(hub.add_sensor_item(var))
