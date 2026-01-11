import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
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


EmersonR48Number = emerson_r48_ns.class_(
    "EmersonR48Number", number.Number, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_EMERSON_R48_ID): cv.use_id(EmersonR48Component),
            cv.Optional(CONF_OUTPUT_VOLTAGE): number._NUMBER_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(EmersonR48Number),
                    cv.Optional(CONF_MIN_VALUE, default=41): cv.float_,
                    cv.Optional(CONF_MAX_VALUE, default=58.5): cv.float_,
                    cv.Optional(CONF_STEP, default=0.1): cv.float_,
                    cv.Optional(CONF_ICON, default=ICON_FLASH): cv.icon,
                    cv.Optional(
                        CONF_UNIT_OF_MEASUREMENT, default=UNIT_VOLT
                    ): cv.string_strict,
                    cv.Optional(CONF_MODE, default="SLIDER"): cv.enum(
                        number.NUMBER_MODES, upper=True
                    ),
                    cv.Optional(
                        CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_NONE
                    ): cv.entity_category,
                }
            ),
            cv.Optional(CONF_MAX_OUTPUT_CURRENT): number._NUMBER_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(EmersonR48Number),
                    cv.Optional(CONF_MIN_VALUE, default=1): cv.float_,
                    cv.Optional(CONF_MAX_VALUE, default=121): cv.float_,
                    cv.Optional(CONF_STEP, default=0.5): cv.float_,
                    cv.Optional(CONF_ICON, default=ICON_FLASH): cv.icon,
                    cv.Optional(
                        CONF_UNIT_OF_MEASUREMENT, default=UNIT_AMPERE
                    ): cv.string_strict,
                    cv.Optional(CONF_MODE, default="SLIDER"): cv.enum(
                        number.NUMBER_MODES, upper=True
                    ),
                    cv.Optional(
                        CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_NONE
                    ): cv.entity_category,
                }
            ),
            cv.Optional(CONF_MAX_INPUT_CURRENT): number._NUMBER_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(EmersonR48Number),
                    cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                    cv.Optional(CONF_MAX_VALUE, default=20): cv.float_,
                    cv.Optional(CONF_STEP, default=0.1): cv.float_,
                    cv.Optional(CONF_ICON, default=ICON_CURRENT_AC): cv.icon,
                    cv.Optional(
                        CONF_UNIT_OF_MEASUREMENT, default=UNIT_AMPERE
                    ): cv.string_strict,
                    cv.Optional(CONF_MODE, default="SLIDER"): cv.enum(
                        number.NUMBER_MODES, upper=True
                    ),
                    cv.Optional(
                        CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_NONE
                    ): cv.entity_category,
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EMERSON_R48_ID])
    if config[CONF_OUTPUT_VOLTAGE]:
        conf = config[CONF_OUTPUT_VOLTAGE]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await number.register_number(
            var,
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(getattr(hub, "set_output_voltage_number")(var))
        cg.add(var.set_parent(hub, 0x0))
    if config[CONF_MAX_OUTPUT_CURRENT]:
        conf = config[CONF_MAX_OUTPUT_CURRENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await number.register_number(
            var,
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(getattr(hub, "set_max_output_current_number")(var))
        cg.add(var.set_parent(hub, 0x3))
    if config[CONF_MAX_INPUT_CURRENT]:
        conf = config[CONF_MAX_INPUT_CURRENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await number.register_number(
            var,
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(getattr(hub, "set_max_input_current_number")(var))
        cg.add(var.set_parent(hub, 0x4))
