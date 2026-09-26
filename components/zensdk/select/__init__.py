import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from .. import CONF_ZENSDK_ID, ZenSdkComponent, zensdk_ns

DEPENDENCIES = ["zensdk"]

ZenSdkSelect = zensdk_ns.class_("ZenSdkSelect", select.Select, cg.Parented.template(ZenSdkComponent))

# Selects are read back from the device on every poll, so (unlike write-only settings) they
# do not need to be restored from flash.
# (config key, zenSDK property, property value of option 0, icon, options)
SELECTS = [
    # acMode: 1 = AC input (charge), 2 = AC output (discharge). Written on its own here; use the
    # `input_limit` / `output_limit` / `power_setpoint` numbers or the outputs to start a smart-mode
    # charge/discharge, which send the complete command set.
    ("ac_mode", "acMode", 1, "mdi:power-plug-battery", ["Charge", "Discharge"]),
    # gridOffMode: 0 standard, 1 economic, 2 closure (zenSDK property table).
    ("grid_off_mode", "gridOffMode", 0, "mdi:power-plug-off", ["Standard", "Economic", "Closure"]),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{
            cv.Optional(key): select.select_schema(ZenSdkSelect, icon=icon)
            for key, _prop, _base, icon, _options in SELECTS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])
    for key, prop, base, _icon, options in SELECTS:
        if key in config:
            s = await select.new_select(config[key], options=options)
            await cg.register_parented(s, hub)
            cg.add(s.set_base(base))
            cg.add(s.set_property(prop))
            cg.add(hub.add_select(prop, base, s))
