import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.canbus import CanbusComponent
from esphome.const import CONF_ID

CONF_CANBUS_ID = "canbus_id"
CONF_EMERSON_R48_ID = "emerson_r48_id"

emerson_r48_ns = cg.esphome_ns.namespace("emerson_r48")
EmersonR48Component = emerson_r48_ns.class_(
    "EmersonR48Component", cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EmersonR48Component),
        cv.Required(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    canbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], canbus)
    await cg.register_component(var, config)
