import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_AMPERE,
    UNIT_VOLT,
)

from .. import CONF_SMARTSOLAR_ID, SmartSolar, smartsolar_ns

DEPENDENCIES = ["smartsolar"]

SmartSolarNumber = smartsolar_ns.class_(
    "SmartSolarNumber", number.Number, cg.Parented.template(SmartSolar)
)

# key -> (NumberKind value, default step, schema). The numeric kinds mirror the C++ enum NumberKind in smartsolar.h.
NUMBERS = {
    "absorption_voltage": (
        0, 0.01,
        dict(unit_of_measurement=UNIT_VOLT, device_class=DEVICE_CLASS_VOLTAGE,
             entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:battery-charging-high"),
    ),
    "float_voltage": (
        1, 0.01,
        dict(unit_of_measurement=UNIT_VOLT, device_class=DEVICE_CLASS_VOLTAGE,
             entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:battery-charging-medium"),
    ),
    "max_charge_current": (
        2, 0.1,
        dict(unit_of_measurement=UNIT_AMPERE, device_class=DEVICE_CLASS_CURRENT,
             entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:current-dc"),
    ),
    "equalization_voltage": (
        3, 0.01,
        dict(unit_of_measurement=UNIT_VOLT, device_class=DEVICE_CLASS_VOLTAGE,
             entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:battery-charging-100"),
    ),
}


def _number_schema(cls, step, **kwargs):
    # min_value / max_value are mandatory on purpose: they are the safety limits of YOUR battery. The component also
    # applies hard limits of its own (5..70 V, 0..100 A) and the charger refuses what it does not accept.
    return number.number_schema(cls, **kwargs).extend(
        {
            cv.Required(CONF_MIN_VALUE): cv.float_,
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP, default=step): cv.positive_float,
        }
    )


def _validate_bounds(config):
    for key in NUMBERS:
        if key in config and config[key][CONF_MIN_VALUE] >= config[key][CONF_MAX_VALUE]:
            raise cv.Invalid(f"{key}: min_value must be lower than max_value")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_SMARTSOLAR_ID): cv.use_id(SmartSolar),
            **{
                cv.Optional(key): _number_schema(SmartSolarNumber, step, **kwargs)
                for key, (_, step, kwargs) in NUMBERS.items()
            },
        }
    ),
    _validate_bounds,
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SMARTSOLAR_ID])
    for key, (kind, _, _) in NUMBERS.items():
        if key in config:
            conf = config[key]
            var = await number.new_number(
                conf,
                min_value=conf[CONF_MIN_VALUE],
                max_value=conf[CONF_MAX_VALUE],
                step=conf[CONF_STEP],
            )
            await cg.register_parented(var, config[CONF_SMARTSOLAR_ID])
            cg.add(var.set_kind(kind))
            cg.add(hub.register_number(var))
