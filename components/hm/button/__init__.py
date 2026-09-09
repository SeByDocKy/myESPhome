import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import HMComponent, hm_ns

CONF_HM_ID = "hm_id"
CONF_RESET_TO_OUTPUT_MIN = "reset_to_output_min"
CONF_RESET_TO_OUTPUT_MAX = "reset_to_output_max"

DEPENDENCIES = ["hm"]

# En dessous de ce seuil, l'onduleur arrête purement et simplement de produire
# (0% = extinction) -- 2% est le minimum qui le laisse actif.
HM_OUTPUT_MIN_PERCENT = 2.0
HM_OUTPUT_MAX_PERCENT = 100.0

HMResetPercentButton = hm_ns.class_("HMResetPercentButton", button.Button)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HM_ID): cv.use_id(HMComponent),
        cv.Optional(CONF_RESET_TO_OUTPUT_MIN): button.button_schema(
            HMResetPercentButton,
            icon="mdi:arrow-collapse-down",
        ),
        cv.Optional(CONF_RESET_TO_OUTPUT_MAX): button.button_schema(
            HMResetPercentButton,
            icon="mdi:arrow-collapse-up",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HM_ID])

    if CONF_RESET_TO_OUTPUT_MIN in config:
        conf = config[CONF_RESET_TO_OUTPUT_MIN]
        var = await button.new_button(conf)
        cg.add(var.set_parent(hub))
        cg.add(var.set_target_percent(HM_OUTPUT_MIN_PERCENT))

    if CONF_RESET_TO_OUTPUT_MAX in config:
        conf = config[CONF_RESET_TO_OUTPUT_MAX]
        var = await button.new_button(conf)
        cg.add(var.set_parent(hub))
        cg.add(var.set_target_percent(HM_OUTPUT_MAX_PERCENT))
