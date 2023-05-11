import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor, time
from esphome.const import (
    CONF_ICON,
    CONF_ID,
    CONF_RESTORE,
    CONF_TIME_ID,
    CONF_ENTITY_ID,
    CONF_METHOD,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS,
)
from esphome.core.entity_helpers import inherit_property_from

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["time"]


statistics_ns = cg.esphome_ns.namespace("statistics")
statistics_method = statistics_ns.enum("statistics_method")
STATISTICS_METHODS = {
    "min": statistics_method.STATISTICS_METHODS_MIN,
    "max": statistics_method.STATISTICS_METHODS_MAX,
    "mean": statistics_method.STATISTICS_METHODS_MEAN,
}
STATISTICSComponent = statistics_ns.class_(
    "STATISTICSComponent", sensor.Sensor, cg.Component
)

# Action
STATISTICSresetaction = statistics_ns.class_(
    "STATISTICSresetaction", automation.Action
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        STATISTICSComponent,
    )
    .extend(
        {
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Required(CONF_ENTITY_ID): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_RESTORE, default=True): cv.boolean,
            cv.Optional(CONF_METHOD, default="min"): cv.enum(
                STATISTICS_METHODS, lower=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(STATISTICSComponent),
            cv.Optional(CONF_ICON): cv.icon,
            cv.Optional(CONF_UNIT_OF_MEASUREMENT): sensor.validate_unit_of_measurement,
            cv.Optional(CONF_ACCURACY_DECIMALS): sensor.validate_accuracy_decimals,
            cv.Required(CONF_ENTITY_ID): cv.use_id(sensor.Sensor),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    inherit_property_from(CONF_ICON, CONF_ENTITY_ID),
    inherit_property_from(
        CONF_UNIT_OF_MEASUREMENT, CONF_ENTITY_ID
    ),
    inherit_property_from(
        CONF_ACCURACY_DECIMALS, CONF_ENTITY_ID
    ),
)

async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    sens = await cg.get_variable(config[CONF_ENTITY_ID])
    cg.add(var.set_parent(sens))
    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
    cg.add(var.set_restore(config[CONF_RESTORE]))
    cg.add(var.set_method(config[CONF_METHOD]))
	
# Action
STATISTICS_RESET_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(STATISTICSComponent),
    }
)

@automation.register_action(
    "statistics.reset",
    STATISTICSresetaction,
    STATISTICS_RESET_SCHEMA,
)	
async def statistics_reset_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var