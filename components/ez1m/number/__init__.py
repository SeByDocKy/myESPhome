import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP, ENTITY_CATEGORY_CONFIG
from .. import ez1m_ns, EZ1MComponent, CONF_EZ1M_ID

DEPENDENCIES = ["ez1m"]

EZ1MNumber = ez1m_ns.class_("EZ1MNumber", number.Number, cg.Parented.template(EZ1MComponent))
EZ1MNumberType = ez1m_ns.enum("EZ1MNumberType", is_class=True)

CONF_POWER_LIMIT = "power_limit"
CONF_TOTAL_ENERGY = "total_energy"

# Icon conventions kept in sync with hms's number/__init__.py (mdi:flash for
# power-limit controls) and sensor/__init__.py (mdi:counter for energy).
NUMBER_TYPES = {
    CONF_POWER_LIMIT: number.number_schema(EZ1MNumber, icon="mdi:flash").extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=30): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=800): cv.float_,
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
            cg.add(var.set_mode(number.NUMBER_MODES["SLIDER"]))
            cg.add(hub.set_power_limit_number(var))
        else:
            cg.add(hub.set_total_energy_number(var))
