import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import CONF_PCM3K6W_ID, PCM3K6W_PLATFORM_SCHEMA, pcm3k6w_ns

DEPENDENCIES = ["canbus"]

PCM3K6WButton = pcm3k6w_ns.class_("PCM3K6WButton", button.Button)

# Kind indices MUST match the ButtonKind enum order in pcm3k6w.h.
KIND_STOP, KIND_START, KIND_RESET, KIND_SET_DEFAULT_EEPROM = range(4)

# (config key, kind, icon)
BUTTONS = [
    ("stop", KIND_STOP, "mdi:stop"),
    ("start", KIND_START, "mdi:play"),
    ("reset", KIND_RESET, "mdi:restart"),
    # Writes, in order: grid mode (EEPROM, reg 0x02) -> charge, then charging
    # current (EEPROM, reg 0x31) and discharging current (EEPROM, reg 0x32)
    # forced to 1.0A. The voltage half of both 0x31/0x32 reuses whatever
    # number.charging_voltage currently reads (or its 56.0V default) - there
    # is no separate discharging voltage entity. See
    # PCM3K6WComponent::write_button()/BTN_SET_DEFAULT_EEPROM.
    ("set_default_eeprom", KIND_SET_DEFAULT_EEPROM, "mdi:content-save-cog"),
]

CONFIG_SCHEMA = PCM3K6W_PLATFORM_SCHEMA.extend(
    {cv.Optional(key): button.button_schema(PCM3K6WButton, icon=icon) for key, _kind, icon in BUTTONS}
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PCM3K6W_ID])
    for key, kind, _icon in BUTTONS:
        if key in config:
            b = await button.new_button(config[key])
            await cg.register_parented(b, hub)
            cg.add(b.set_kind(kind))
