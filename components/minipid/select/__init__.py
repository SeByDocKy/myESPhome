import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import ENTITY_CATEGORY_CONFIG

DEPENDENCIES = ["minipid"]

from .. import CONF_MINIPID_ID, MINIPIDComponent, minipid_ns

ICON_MULTIPLIER = "mdi:multiplication"

CoeffMultiplierSelect = minipid_ns.class_("CoeffMultiplierSelect", select.Select, cg.Component)

CONF_COEFF_MULTIPLIER = "coeff_multiplier"

# Options must stay in this order: the C++ side maps the option text to a float.
COEFF_MULTIPLIER_OPTIONS = ["10", "1", "0.1", "0.01"]
COEFF_MULTIPLIER_DEFAULT_INDEX = 1  # "1"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MINIPID_ID): cv.use_id(MINIPIDComponent),

    cv.Optional(CONF_COEFF_MULTIPLIER): select.select_schema(
        CoeffMultiplierSelect,
        icon=ICON_MULTIPLIER,
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend(cv.COMPONENT_SCHEMA),
}


async def to_code(config):
    minipid_component = await cg.get_variable(config[CONF_MINIPID_ID])
    if coeff_multiplier_config := config.get(CONF_COEFF_MULTIPLIER):
        s = await select.new_select(
            coeff_multiplier_config, options=COEFF_MULTIPLIER_OPTIONS
        )
        await cg.register_component(s, coeff_multiplier_config)
        await cg.register_parented(s, minipid_component)
        cg.add(s.set_initial_index(COEFF_MULTIPLIER_DEFAULT_INDEX))
        cg.add(minipid_component.set_coeff_multiplier_select(s))
