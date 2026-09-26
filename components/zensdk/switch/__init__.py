import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import CONF_ZENSDK_ID, ZenSdkComponent, zensdk_ns

DEPENDENCIES = ["zensdk"]

ZenSdkSwitch = zensdk_ns.class_("ZenSdkSwitch", switch.Switch, cg.Parented.template(ZenSdkComponent))

# Switches are read back from the device on every poll and are not restored at boot
# (the class is not a Component), so nothing is written to the battery on startup.
# (config key, zenSDK property, icon)
SWITCHES = [
    # LED strip of the device. Zendure-HA exposes `lampSwitch` as a writable switch ("LED") and writes
    # 1 / 0 with POST /properties/write, although the zenSDK property table lists it as read-only.
    ("lamp", "lampSwitch", "mdi:led-strip-variant"),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{cv.Optional(key): switch.switch_schema(ZenSdkSwitch, icon=icon) for key, _prop, icon in SWITCHES},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])
    for key, prop, _icon in SWITCHES:
        if key in config:
            s = await switch.new_switch(config[key])
            await cg.register_parented(s, hub)
            cg.add(s.set_property(prop))
            cg.add(hub.add_switch(prop, s))
