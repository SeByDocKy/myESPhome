import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import HMSComponent, hms_ns

CONF_HMS_ID = "hms_id"
CONF_RESET_TO_OUTPUT_MIN = "reset_to_output_min"
CONF_RESET_TO_OUTPUT_MAX = "reset_to_output_max"
CONF_RESET_HMS = "reset_hms"

DEPENDENCIES = ["hms"]

# En dessous de ce seuil, le HMS arrête purement et simplement de produire (0% =
# extinction) -- 2% est le minimum qui le laisse actif tout en écrêtant fortement.
HMS_OUTPUT_MIN_PERCENT = 2.0
HMS_OUTPUT_MAX_PERCENT = 100.0

HMSResetPercentButton = hms_ns.class_("HMSResetPercentButton", button.Button)
HMSResetHmsButton = hms_ns.class_("HMSResetHmsButton", button.Button)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_ID): cv.use_id(HMSComponent),
        cv.Optional(CONF_RESET_TO_OUTPUT_MIN): button.button_schema(
            HMSResetPercentButton,
            icon="mdi:arrow-collapse-down",
        ),
        cv.Optional(CONF_RESET_TO_OUTPUT_MAX): button.button_schema(
            HMSResetPercentButton,
            icon="mdi:arrow-collapse-up",
        ),
        cv.Optional(CONF_RESET_HMS): button.button_schema(
            HMSResetHmsButton,
            icon="mdi:restart",
            entity_category="diagnostic",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_ID])

    if CONF_RESET_TO_OUTPUT_MIN in config:
        conf = config[CONF_RESET_TO_OUTPUT_MIN]
        var = await button.new_button(conf)
        cg.add(var.set_parent(hub))
        cg.add(var.set_target_percent(HMS_OUTPUT_MIN_PERCENT))

    if CONF_RESET_TO_OUTPUT_MAX in config:
        conf = config[CONF_RESET_TO_OUTPUT_MAX]
        var = await button.new_button(conf)
        cg.add(var.set_parent(hub))
        cg.add(var.set_target_percent(HMS_OUTPUT_MAX_PERCENT))

    if CONF_RESET_HMS in config:
        conf = config[CONF_RESET_HMS]
        var = await button.new_button(conf)
        cg.add(var.set_parent(hub))
