import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import (
    CONF_DEYE_MONO_ID,
    DEYE_MONO_PLATFORM_SCHEMA,
    add_register_item,
    deye_mono_ns,
    entity_kwargs,
)
from ..registers import BINARY_SENSORS

DEPENDENCIES = ["deye_mono"]

DeyeMonoBinarySensor = deye_mono_ns.class_("DeyeMonoBinarySensor", binary_sensor.BinarySensor, cg.Component)

CONFIG_SCHEMA = DEYE_MONO_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): binary_sensor.binary_sensor_schema(
            DeyeMonoBinarySensor, **entity_kwargs(spec, "icon", "entity_category", "device_class")
        )
        for key, spec in BINARY_SENSORS.items()
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYE_MONO_ID])

    for key, conf in config.items():
        if key not in BINARY_SENSORS:
            continue
        spec = BINARY_SENSORS[key]
        var = await binary_sensor.new_binary_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_mask(spec["mask"]))
        add_register_item(hub, var, spec)
