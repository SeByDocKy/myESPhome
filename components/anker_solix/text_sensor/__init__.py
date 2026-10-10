import esphome.codegen as cg
from esphome.components import text_sensor
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
from ..registers import MODEL_NAMES, PRODUCT_CODES, REGISTERS

DEPENDENCIES = ["anker_solix"]

AnkerSolixTextSensor = anker_solix_ns.class_("AnkerSolixTextSensor", text_sensor.TextSensor, cg.Component)
TextKind = anker_solix_ns.enum("TextKind", True)

KINDS = {
    "char": TextKind.CHAR,
    "states": TextKind.STATES,
    "product": TextKind.PRODUCT,
}

SPECS = union_specs("text_sensor")

CONFIG_SCHEMA = ANKER_SOLIX_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): text_sensor.text_sensor_schema(
            AnkerSolixTextSensor, **entity_kwargs(spec, "icon", "entity_category")
        )
        for key, spec in SPECS.items()
    }
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("text_sensor")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ANKER_SOLIX_ID])
    hub_conf = hub_config(config[CONF_ANKER_SOLIX_ID])
    model = hub_conf[CONF_MODEL]
    specs = REGISTERS[model]["text_sensor"]

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)

        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                MODBUS_REGISTER_TYPE[spec["rtype"]],
            )
        )
        cg.add(var.set_kind(KINDS[spec["kind"]]))
        for value, label in spec.get("states", {}).items():
            cg.add(var.add_state(value, label))
        if spec["kind"] == "product":
            products = PRODUCT_CODES[model]
            for code, name in products["codes"].items():
                cg.add(var.add_product(code, name))
            cg.add(var.set_default_name(products["default"]))
            cg.add(var.set_model_name(MODEL_NAMES[model]))
        cg.add(hub.add_item(var, is_slow(spec)))
