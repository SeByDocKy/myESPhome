import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number

from .. import CMT2300AComponent, cmt2300a_ns

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["cmt2300a"]

CONF_CMT2300A_ID = "cmt2300a_id"
CONF_PA_LEVEL = "pa_level"

CMT2300APALevelNumber = cmt2300a_ns.class_("CMT2300APALevelNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CMT2300A_ID): cv.use_id(CMT2300AComponent),
        cv.Optional(CONF_PA_LEVEL): number.number_schema(
            CMT2300APALevelNumber,
            icon="mdi:signal-cellular-3",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_CMT2300A_ID])

    if CONF_PA_LEVEL in config:
        conf = config[CONF_PA_LEVEL]
        n = await number.new_number(conf, min_value=-10, max_value=20, step=1)
        cg.add(n.set_parent(hub))
        cg.add(hub.set_pa_level_number(n))
