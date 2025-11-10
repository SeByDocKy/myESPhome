import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import (
    UNIT_VOLT,
    CONF_ID,
    CONF_ICON,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_MODE,
    CONF_ENTITY_CATEGORY,
    ICON_FLASH,
    ICON_CURRENT_AC,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
    UNIT_AMPERE,
    ENTITY_CATEGORY_NONE,
)

from .. import EmersonR48Component, emerson_r48_ns, CONF_EMERSON_R48_ID

CONF_OUTPUT_VOLTAGE = "output_voltage"
CONF_MAX_OUTPUT_CURRENT = "max_output_current"
CONF_MAX_INPUT_CURRENT = "max_input_current"


EmersonR48Output = emerson_r48_ns.class_(
    "EmersonR48Output", output.FloatOutput, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_EMERSON_R48_ID): cv.use_id(EmersonR48Component),
            cv.Optional(CONF_OUTPUT_VOLTAGE): output.FLOAT_OUTPUT_SCHEMA.extend({
              cv.Required(CONF_ID): cv.declare_id(EmersonR48Output),
            }),
            cv.Optional(CONF_MAX_OUTPUT_CURRENT): output.FLOAT_OUTPUT_SCHEMA.extend({
              cv.Required(CONF_ID): cv.declare_id(EmersonR48Output),
            }),
            cv.Optional(CONF_MAX_INPUT_CURRENT): output.FLOAT_OUTPUT_SCHEMA.extend({
              cv.Required(CONF_ID): cv.declare_id(EmersonR48Output),
            }),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EMERSON_R48_ID])
    if config[CONF_OUTPUT_VOLTAGE]: 
        conf = config[CONF_OUTPUT_VOLTAGE]
        out = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(out, conf)
        cg.add(getattr(hub, "set_output_voltage_output")(out))
        cg.add(out.set_parent(hub, 0x0))
        
     
    if config[CONF_MAX_OUTPUT_CURRENT]:
        conf = config[CONF_OUTPUT_VOLTAGE]
        out = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(out, conf)
        cg.add(getattr(hub, "set_max_output_current_output")(out))
        cg.add(out.set_parent(hub, 0x3))
        
 
    if config[CONF_MAX_INPUT_CURRENT]: 
        conf = config[CONF_OUTPUT_VOLTAGE]
        out = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(out, conf)
        cg.add(getattr(hub, "set_max_input_current_output")(out))
        cg.add(out.set_parent(hub, 0x4))
        
