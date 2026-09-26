import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_ZENSDK_ID, ZenSdkComponent, zensdk_ns

DEPENDENCIES = ["zensdk"]

ZenSdkSwitch = zensdk_ns.class_("ZenSdkSwitch", switch.Switch, cg.Parented.template(ZenSdkComponent))
ZenSdkKickstartSwitch = zensdk_ns.class_(
    "ZenSdkKickstartSwitch", switch.Switch, cg.Component, cg.Parented.template(ZenSdkComponent)
)

CONF_KICKSTART = "kickstart"

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
        # Software-only: enables the kick-start boost for a +/-50 W request the device does not follow yet
        # (logic taken from Zendure-HA). Restored across reboots, off by default.
        cv.Optional(CONF_KICKSTART): switch.switch_schema(
            ZenSdkKickstartSwitch,
            icon="mdi:rocket-launch",
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ).extend(cv.COMPONENT_SCHEMA),
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
    if CONF_KICKSTART in config:
        conf = config[CONF_KICKSTART]
        s = await switch.new_switch(conf)
        await cg.register_component(s, conf)
        await cg.register_parented(s, hub)
