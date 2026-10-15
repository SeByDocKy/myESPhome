import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import number, select
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_OPTIONS,
    CONF_STEP,
)
from esphome.core import Lambda

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["api"]

CONF_MIN_INTERVAL = "min_interval"
CONF_DELAY = "delay"
CONF_NUMBER_ID = "number_id"
CONF_CLAMP_STATE = "clamp_state"
CONF_SELECT_ID = "select_id"
CONF_FALLBACK_INDEX = "fallback_index"

api_resync_ns = cg.esphome_ns.namespace("api_resync")
ApiResyncComponent = api_resync_ns.class_("ApiResyncComponent", cg.Component)
ResyncAction = api_resync_ns.class_("ResyncAction", automation.Action)
SetNumberRangeAction = api_resync_ns.class_("SetNumberRangeAction", automation.Action)
SetSelectOptionsAction = api_resync_ns.class_(
    "SetSelectOptionsAction", automation.Action
)

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


SET_SELECT_OPTIONS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ApiResyncComponent),
        cv.Required(CONF_SELECT_ID): cv.use_id(select.Select),
        cv.Required(CONF_OPTIONS): cv.Any(
            cv.returning_lambda,
            cv.All(cv.ensure_list(cv.string_strict), cv.Length(min=1)),
        ),
        cv.Optional(CONF_FALLBACK_INDEX, default=0): cv.positive_int,
    }
)


@automation.register_action(
    "api_resync.set_select_options",
    SetSelectOptionsAction,
    SET_SELECT_OPTIONS_SCHEMA,
)
async def set_select_options_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    sel = await cg.get_variable(config[CONF_SELECT_ID])
    cg.add(var.set_select(sel))
    cg.add(var.set_fallback_index(config[CONF_FALLBACK_INDEX]))
    options = config[CONF_OPTIONS]
    if isinstance(options, Lambda):
        template_ = await cg.process_lambda(
            options, args, return_type=cg.std_vector.template(cg.std_string)
        )
        cg.add(var.set_options_lambda(template_))
    else:
        cg.add(var.set_options_static(options))
    return var
