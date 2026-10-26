import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_PERCENT

from .. import CONF_DEYEMI_ID, DeyeMiComponent, deyemi_ns

DEPENDENCIES = ["deyemi"]

CONF_POWER_PERSISTENT = "power_persistent"
CONF_POWER_NON_PERSISTENT = "power_non_persistent"

# power_persistent: register 0x0028, stored in the inverter's non-volatile memory.
DeyeMiPowerPersistentNumber = deyemi_ns.class_(
    "DeyeMiPowerPersistentNumber", number.Number, cg.Parented.template(DeyeMiComponent)
)
# power_non_persistent: register 0x0035, RAM only (not persisted).
DeyeMiPowerNonPersistentNumber = deyemi_ns.class_(
    "DeyeMiPowerNonPersistentNumber", number.Number, cg.Parented.template(DeyeMiComponent)
)


def _schema(cls, icon):
    return number.number_schema(
        cls,
        icon=icon,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_PERCENT,
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYEMI_ID): cv.use_id(DeyeMiComponent),
        cv.Optional(CONF_POWER_PERSISTENT): _schema(DeyeMiPowerPersistentNumber, "mdi:percent"),
        cv.Optional(CONF_POWER_NON_PERSISTENT): _schema(DeyeMiPowerNonPersistentNumber, "mdi:percent-outline"),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYEMI_ID])
    if CONF_POWER_PERSISTENT in config:
        var = await number.new_number(config[CONF_POWER_PERSISTENT], min_value=0.0, max_value=120.0, step=1.0)
        await cg.register_parented(var, hub)
        cg.add(hub.set_power_percent_number(var))
    if CONF_POWER_NON_PERSISTENT in config:
        var = await number.new_number(config[CONF_POWER_NON_PERSISTENT], min_value=0.0, max_value=120.0, step=1.0)
        await cg.register_parented(var, hub)
