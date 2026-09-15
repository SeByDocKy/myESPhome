import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from .. import ez1m_ns, EZ1MComponent, CONF_EZ1M_ID

DEPENDENCIES = ["ez1m"]

EZ1MSwitch = ez1m_ns.class_(
    "EZ1MSwitch", switch.Switch, cg.Component, cg.Parented.template(EZ1MComponent)
)

CONF_INVERTER_ONOFF = "inverter_onoff"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EZ1M_ID): cv.use_id(EZ1MComponent),
        cv.Optional(CONF_INVERTER_ONOFF): switch.switch_schema(
            EZ1MSwitch,
            icon="mdi:power",
            default_restore_mode="RESTORE_DEFAULT_ON",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EZ1M_ID])
    if CONF_INVERTER_ONOFF in config:
        conf = config[CONF_INVERTER_ONOFF]
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(hub.set_onoff_switch(var))
