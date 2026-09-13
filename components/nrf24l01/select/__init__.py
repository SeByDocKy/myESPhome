import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from .. import NRF24Component, nrf24l01_ns

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["nrf24l01"]

CONF_NRF24L01_ID = "nrf24l01_id"
CONF_PA_LEVEL = "pa_level"

NRF24PALevelSelect = nrf24l01_ns.class_("NRF24PALevelSelect", select.Select)

_PA_LEVEL_OPTIONS = ["min", "low", "high", "max"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NRF24L01_ID): cv.use_id(NRF24Component),
        cv.Optional(CONF_PA_LEVEL): select.select_schema(
            NRF24PALevelSelect,
            icon="mdi:signal-cellular-3",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NRF24L01_ID])

    if CONF_PA_LEVEL in config:
        conf = config[CONF_PA_LEVEL]
        s = await select.new_select(conf, options=_PA_LEVEL_OPTIONS)
        cg.add(s.set_parent(hub))
        cg.add(hub.set_pa_level_select(s))
