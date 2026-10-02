import esphome.codegen as cg
from esphome.components import text_sensor
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

MarstekTextSensor = marstek_ns.class_("MarstekTextSensor", text_sensor.TextSensor, cg.Component)
MarstekFirmwareTextSensor = marstek_ns.class_(
    "MarstekFirmwareTextSensor", text_sensor.TextSensor, cg.Component
)
TextKind = marstek_ns.enum("TextKind", True)

KINDS = {
    "char": TextKind.CHAR,
    "mac": TextKind.MAC,
    "ipv4": TextKind.IPV4,
    "states": TextKind.STATES,
    "bits": TextKind.BITS,
}
# Slot of each source register in MarstekFirmwareTextSensor
FIRMWARE_SLOTS = {"ems": 0, "bms": 1, "vms": 2}

SPECS = union_specs("text_sensor")


def _schema(spec):
    cls = MarstekFirmwareTextSensor if spec["kind"] == "firmware" else MarstekTextSensor
    return text_sensor.text_sensor_schema(
        cls, **entity_kwargs(spec, "icon", "entity_category")
    )


CONFIG_SCHEMA = MARSTEK_PLATFORM_SCHEMA.extend(
    {cv.Optional(key): _schema(spec) for key, spec in SPECS.items()}
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("text_sensor")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MARSTEK_ID])
    hub_conf = hub_config(config[CONF_MARSTEK_ID])
    specs = REGISTERS[hub_conf[CONF_MODEL]]["text_sensor"]

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)

        if spec["kind"] == "firmware":
            cg.add(var.set_with_vms(spec["mode"] == "ems_vms_bms"))
            for alias, dep in spec["deps"].items():
                index = FIRMWARE_SLOTS[alias]
                cg.add(
                    var.configure_dep(
                        index,
                        dep["register"],
                        dep["count"],
                        SENSOR_VALUE_TYPE[dep["vtype"]],
                        dep["scale"],
                        skip_updates(dep, hub_conf),
                    )
                )
                cg.add(hub.add_sensor_item(var.get_dep(index)))
            continue

        vtype = "U_WORD" if spec["kind"] == "states" else "RAW"
        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[vtype],
                skip_updates(spec, hub_conf),
            )
        )
        cg.add(var.set_kind(KINDS[spec["kind"]]))
        for value, label in spec.get("states", {}).items():
            cg.add(var.add_state(value, label))
        for bit, name in spec.get("bits", {}).items():
            cg.add(var.add_bit(bit, name))
        cg.add(hub.add_sensor_item(var))
