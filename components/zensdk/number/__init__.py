import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_PERCENT, UNIT_WATT

from .. import CONF_ZENSDK_ID, ZenSdkComponent, zensdk_ns

DEPENDENCIES = ["zensdk"]

ZenSdkNumber = zensdk_ns.class_("ZenSdkNumber", number.Number, cg.Parented.template(ZenSdkComponent))

# Kind ids MUST match the NumberKind enum in zensdk.h.
KIND_PROPERTY, KIND_SOC, KIND_INPUT_LIMIT, KIND_OUTPUT_LIMIT, KIND_POWER_SETPOINT = range(5)

# Slider ranges cover the largest supported model (charge 3200 W / discharge 2400 W); the hub clamps
# every request to the limits of the configured model and publishes the clamped value back.
# (config key, zenSDK property, kind, min, max, step, unit, icon, entity_category)
NUMBERS = [
    ("input_limit", "inputLimit", KIND_INPUT_LIMIT, 0, 3200, 1, UNIT_WATT, "mdi:battery-arrow-up", None),
    ("output_limit", "outputLimit", KIND_OUTPUT_LIMIT, 0, 2400, 1, UNIT_WATT, "mdi:battery-arrow-down", None),
    # Signed: > 0 discharge, < 0 charge, 0 stop.
    ("power_setpoint", "", KIND_POWER_SETPOINT, -3200, 2400, 1, UNIT_WATT, "mdi:swap-vertical", None),
    ("soc_set", "socSet", KIND_SOC, 70, 100, 1, UNIT_PERCENT, "mdi:percent", ENTITY_CATEGORY_CONFIG),
    ("min_soc", "minSoc", KIND_SOC, 0, 50, 1, UNIT_PERCENT, "mdi:percent", ENTITY_CATEGORY_CONFIG),
    (
        "inverse_max_power",
        "inverseMaxPower",
        KIND_PROPERTY,
        0,
        2400,
        1,
        UNIT_WATT,
        "mdi:power",
        ENTITY_CATEGORY_CONFIG,
    ),
]


def _schema(icon, unit, category):
    kwargs = dict(icon=icon, unit_of_measurement=unit)
    if category is not None:
        kwargs["entity_category"] = category
    return number.number_schema(ZenSdkNumber, **kwargs)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{
            cv.Optional(key): _schema(icon, unit, category)
            for key, _prop, _kind, _min, _max, _step, unit, icon, category in NUMBERS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])
    for key, prop, kind, min_value, max_value, step, _unit, _icon, _category in NUMBERS:
        if key in config:
            var = await number.new_number(
                config[key], min_value=float(min_value), max_value=float(max_value), step=float(step)
            )
            await cg.register_parented(var, hub)
            cg.add(var.set_kind(kind))
            cg.add(var.set_property(prop))
            cg.add(hub.add_number(kind, prop, var))
