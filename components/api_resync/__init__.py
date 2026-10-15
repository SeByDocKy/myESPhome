import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import number
from esphome.const import CONF_ID, CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["api"]

CONF_MIN_INTERVAL = "min_interval"
CONF_DELAY = "delay"
CONF_NUMBER_ID = "number_id"
CONF_CLAMP_STATE = "clamp_state"

api_resync_ns = cg.esphome_ns.namespace("api_resync")
ApiResyncComponent = api_resync_ns.class_("ApiResyncComponent", cg.Component)
ResyncAction = api_resync_ns.class_("ResyncAction", automation.Action)
SetNumberRangeAction = api_resync_ns.class_("SetNumberRangeAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ApiResyncComponent),
        cv.Optional(
            CONF_MIN_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DELAY, default="500ms"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_min_interval(config[CONF_MIN_INTERVAL]))
    cg.add(var.set_delay(config[CONF_DELAY]))


RESYNC_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(ApiResyncComponent),
    }
)


@automation.register_action("api_resync.resync", ResyncAction, RESYNC_ACTION_SCHEMA)
async def resync_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


SET_NUMBER_RANGE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ApiResyncComponent),
        cv.Required(CONF_NUMBER_ID): cv.use_id(number.Number),
        cv.Required(CONF_MIN_VALUE): cv.templatable(cv.float_),
        cv.Required(CONF_MAX_VALUE): cv.templatable(cv.float_),
        cv.Optional(CONF_STEP): cv.templatable(cv.positive_float),
        cv.Optional(CONF_CLAMP_STATE, default=True): cv.boolean,
    }
)


@automation.register_action(
    "api_resync.set_number_range", SetNumberRangeAction, SET_NUMBER_RANGE_SCHEMA
)
async def set_number_range_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    num = await cg.get_variable(config[CONF_NUMBER_ID])
    cg.add(var.set_number(num))
    cg.add(var.set_clamp_state(config[CONF_CLAMP_STATE]))
    template_ = await cg.templatable(config[CONF_MIN_VALUE], args, float)
    cg.add(var.set_min_value(template_))
    template_ = await cg.templatable(config[CONF_MAX_VALUE], args, float)
    cg.add(var.set_max_value(template_))
    if CONF_STEP in config:
        template_ = await cg.templatable(config[CONF_STEP], args, float)
        cg.add(var.set_step(template_))
    return var
