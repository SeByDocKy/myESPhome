import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from .. import CONF_PCM3K6W_ID, PCM3K6W_PLATFORM_SCHEMA, pcm3k6w_ns

DEPENDENCIES = ["canbus"]

PCM3K6WSelect = pcm3k6w_ns.class_("PCM3K6WSelect", select.Select, cg.Component)

# Kind indices MUST match the SelectKind enum order in pcm3k6w.h.
KIND_GRID_MODE, KIND_PHASE_MODE_SETTING, KIND_FREQUENCY_SETTING, KIND_AC_VOLTAGE_SETTING = range(4)

# (config key, kind, icon, options, initial index)
SELECTS = [
    ("grid_mode", KIND_GRID_MODE, "mdi:power-plug-battery", ["Charge mode", "Discharge mode"], 0),
    ("phase_mode_setting", KIND_PHASE_MODE_SETTING, "mdi:electric-switch", ["Single-Phase", "Three-Phase"], 0),
    ("frequency_setting", KIND_FREQUENCY_SETTING, "mdi:metronome", ["50 Hz", "60 Hz"], 0),
    ("ac_voltage_setting", KIND_AC_VOLTAGE_SETTING, "mdi:sine-wave", ["230V", "240V"], 0),
]

CONFIG_SCHEMA = PCM3K6W_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): select.select_schema(PCM3K6WSelect, icon=icon)
        for key, _kind, icon, _options, _initial in SELECTS
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PCM3K6W_ID])
    for key, kind, _icon, options, initial_index in SELECTS:
        if key in config:
            s = await select.new_select(config[key], options=options)
            await cg.register_component(s, config[key])
            await cg.register_parented(s, hub)
            cg.add(s.set_kind(kind))
            cg.add(s.set_initial_index(initial_index))
            cg.add(hub.set_select(kind, s))
