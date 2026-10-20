import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv

from .. import CONF_OPENFIRENET_ID, OpenFireNetComponent, openfirenet_ns

DEPENDENCIES = ["openfirenet"]

OpenFireNetModeSelect = openfirenet_ns.class_(
    "OpenFireNetModeSelect", select.Select, cg.Parented.template(OpenFireNetComponent)
)

CONF_MODE = "mode"

# Index == stove mode number.
MODE_OPTIONS = ["Manual", "Auto", "Comfort", "Setback"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENFIRENET_ID): cv.use_id(OpenFireNetComponent),
        cv.Optional(CONF_MODE): select.select_schema(OpenFireNetModeSelect, icon="mdi:cog-outline"),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENFIRENET_ID])
    if conf := config.get(CONF_MODE):
        s = await select.new_select(conf, options=MODE_OPTIONS)
        await cg.register_parented(s, hub)
        cg.add(hub.set_mode_select(s))
