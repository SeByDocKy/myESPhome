import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
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

CONF_AC_SWITCH = "ac_sw"
CONF_DC_SWITCH = "dc_sw"
CONF_FAN_SWITCH = "fan_sw"
CONF_LED_SWITCH = "led_sw"


EmersonR48Switch = emerson_r48_ns.class_(
    "EmersonR48Switch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_EMERSON_R48_ID): cv.use_id(EmersonR48Component),
            cv.Optional(CONF_AC_SWITCH): switch.SWITCH_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(EmersonR48Switch),
                    cv.Optional(CONF_ICON, default=ICON_FLASH): cv.icon,
#                    cv.Optional(CONF_MODE, default="BOX"): cv.enum(
#                        switch.SWITCH_MODES, upper=True
#                    ),
                    cv.Optional(
                        CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_NONE
                    ): cv.entity_category,
                }
            ),
            cv.Optional(CONF_DC_SWITCH): switch.switch_schema(
                {
                    cv.GenerateID(): cv.declare_id(EmersonR48Switch),
                    cv.Optional(CONF_ICON, default=ICON_CURRENT_AC): cv.icon,
#                    cv.Optional(CONF_MODE, default="BOX"): cv.enum(
#                        switch.SWITCH_MODES, upper=True
#                    ),
                    cv.Optional(
                        CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_NONE
                    ): cv.entity_category,
                }
            ),
            cv.Optional(CONF_FAN_SWITCH): switch.switch_schema(
                {
                    cv.GenerateID(): cv.declare_id(EmersonR48Switch),
                    cv.Optional(CONF_ICON, default=ICON_CURRENT_AC): cv.icon,
#                    cv.Optional(CONF_MODE, default="BOX"): cv.enum(
#                        switch.SWITCH_MODES, upper=True
#                    ),
                    cv.Optional(
                        CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_NONE
                    ): cv.entity_category,
                }
            ),
            cv.Optional(CONF_LED_SWITCH): switch.switch_schema(
                {
                    cv.GenerateID(): cv.declare_id(EmersonR48Switch),
                    cv.Optional(CONF_ICON, default=ICON_CURRENT_AC): cv.icon,
#                    cv.Optional(CONF_MODE, default="BOX"): cv.enum(
#                        switch.SWITCH_MODES, upper=True
#                    ),
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
    if config[CONF_AC_SWITCH]:
        conf = config[CONF_AC_SWITCH]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await switch.register_switch(
            var,
            conf
        )
#        cg.add(getattr(hub, "set_output_voltage_number")(var))
        cg.add(var.set_parent(hub, 0x0))

    if config[CONF_DC_SWITCH]:
        conf = config[CONF_DC_SWITCH]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await switch.register_switch(
            var,
            conf,
        )
#        cg.add(getattr(hub, "set_max_output_current_number")(var))
        cg.add(var.set_parent(hub, 0x1))

    if config[CONF_FAN_SWITCH]:
        conf = config[CONF_FAN_SWITCH]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await switch.register_switch(
            var,
            conf,
        )
#        cg.add(getattr(hub, "set_max_output_current_number")(var))
        cg.add(var.set_parent(hub, 0x2))

    if config[CONF_LED_SWITCH]:
        conf = config[CONF_LED_SWITCH]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await switch.register_switch(
            var,
            conf,
        )
#        cg.add(getattr(hub, "set_max_output_current_number")(var))
        cg.add(var.set_parent(hub, 0x3))

