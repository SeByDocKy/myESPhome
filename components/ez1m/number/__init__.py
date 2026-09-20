import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import number
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODEL,
    CONF_STEP,
    ENTITY_CATEGORY_CONFIG,
    UNIT_WATT,
)
from .. import ez1m_ns, EZ1MComponent, CONF_EZ1M_ID, MODEL_MAX_WATTS

DEPENDENCIES = ["ez1m"]

EZ1MNumber = ez1m_ns.class_("EZ1MNumber", number.Number, cg.Parented.template(EZ1MComponent))
EZ1MNumberType = ez1m_ns.enum("EZ1MNumberType", is_class=True)

CONF_POWER_LIMIT = "power_limit"
CONF_TOTAL_ENERGY = "total_energy"

# power_limit: unit_of_measurement=W, icon mdi:power (matches the hub's own
# power sensors -- ac_power/ch1_dc_power/etc -- rather than hms's mdi:flash,
# per explicit override). total_energy: mdi:counter, in line with the energy
# sensors in sensor/__init__.py.
NUMBER_TYPES = {
    CONF_POWER_LIMIT: number.number_schema(
        EZ1MNumber, unit_of_measurement=UNIT_WATT, icon="mdi:power"
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=30): cv.float_,
            # No static default: the real default is the hub's model-dependent
            # max power (800/960/1800 W for ez1m/ez1h/ez1d), filled in by
            # _final_validate below once the referenced hub's `model` is
            # known. Set this explicitly in YAML to override either way.
            cv.Optional(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP, default=1): cv.float_,
        }
    ),
    CONF_TOTAL_ENERGY: number.number_schema(
        EZ1MNumber,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:counter",
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=999999): cv.float_,
            cv.Optional(CONF_STEP, default=0.001): cv.float_,
        }
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EZ1M_ID): cv.use_id(EZ1MComponent),
        **{cv.Optional(key): schema for key, schema in NUMBER_TYPES.items()},
    }
)


def _final_validate(config):
    # Fill in power_limit's max_value from the referenced hub's `model`,
    # unless the user already set it explicitly in YAML.
    if CONF_POWER_LIMIT not in config:
        return config
    pl_conf = config[CONF_POWER_LIMIT]
    if CONF_MAX_VALUE in pl_conf:
        return config

    fconf = fv.full_config.get()
    hub_path = fconf.get_path_for_id(config[CONF_EZ1M_ID])[:-1]
    hub_conf = fconf.get_config_for_path(hub_path)
    model = hub_conf.get(CONF_MODEL, "ez1m")
    pl_conf[CONF_MAX_VALUE] = MODEL_MAX_WATTS[model]
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EZ1M_ID])
    for key in NUMBER_TYPES:
        if key not in config:
            continue
        conf = config[key]
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        await cg.register_parented(var, hub)
        cg.add(var.set_kind(getattr(EZ1MNumberType, key.upper())))
        if key == CONF_POWER_LIMIT:
            # Forced (not user-configurable): a 30-800 W control range is far
            # more usable as a slider than a text box in the frontend.
            cg.add(var.traits.set_mode(number.NUMBER_MODES["SLIDER"]))
            cg.add(hub.set_power_limit_number(var))
        else:
            cg.add(hub.set_total_energy_number(var))
