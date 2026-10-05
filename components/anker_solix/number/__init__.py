import esphome.codegen as cg
from esphome.components import number
from esphome.components.modbustcp_controller import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE
import esphome.config_validation as cv

from .. import (
    CONF_ANKER_SOLIX_ID,
    CONF_MODEL,
    ANKER_SOLIX_PLATFORM_SCHEMA,
    anker_solix_ns,
    entity_kwargs,
    final_validate_platform,
    hub_config,
    skip_updates,
    union_specs,
)
from ..registers import REGISTERS

DEPENDENCIES = ["anker_solix"]

AnkerPowerNumber = anker_solix_ns.class_("AnkerPowerNumber", number.Number, cg.Component)
AnkerSocNumber = anker_solix_ns.class_("AnkerSocNumber", number.Number, cg.Component)

SPECS = union_specs("number")


def _schema(spec):
    cls = AnkerPowerNumber if spec["kind"] == "power_setpoint" else AnkerSocNumber
    return number.number_schema(
        cls,
        **entity_kwargs(spec, "unit_of_measurement", "icon", "entity_category"),
    )


CONFIG_SCHEMA = ANKER_SOLIX_PLATFORM_SCHEMA.extend(
    {cv.Optional(key): _schema(spec) for key, spec in SPECS.items()}
)
FINAL_VALIDATE_SCHEMA = final_validate_platform("number")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ANKER_SOLIX_ID])
    hub_conf = hub_config(config[CONF_ANKER_SOLIX_ID])
    specs = REGISTERS[hub_conf[CONF_MODEL]]["number"]

    for key, conf in config.items():
        if key not in specs:
            continue
        spec = specs[key]
        var = await number.new_number(
            conf,
            min_value=spec["min"],
            max_value=spec["max"],
            step=spec["step"],
        )
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)

        if spec["kind"] == "power_setpoint":
            # Write-only: the hub needs the power limits of the battery to clamp the request
            cg.add(hub.enable_power_limits())
            continue

        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                MODBUS_REGISTER_TYPE[spec["rtype"]],
                skip_updates(spec, hub_conf),
            )
        )
        cg.add(var.set_rules(spec["capability_bit"], spec["needs_backup_enable"]))
        cg.add(hub.enable_soc_rules())
        cg.add(hub.add_sensor_item(var))
