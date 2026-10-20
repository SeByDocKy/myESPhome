import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from .. import CONF_OPENFIRENET_ID, OpenFireNetComponent, openfirenet_ns

DEPENDENCIES = ["openfirenet"]

OpenFireNetPowerSwitch = openfirenet_ns.class_(
    "OpenFireNetPowerSwitch", switch.Switch, cg.Component, cg.Parented.template(OpenFireNetComponent)
)

CONF_POWER = "power"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENFIRENET_ID): cv.use_id(OpenFireNetComponent),
        cv.Optional(CONF_POWER): switch.switch_schema(
            OpenFireNetPowerSwitch, icon="mdi:power", default_restore_mode="DISABLED"
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENFIRENET_ID])
    if conf := config.get(CONF_POWER):
        sw = await switch.new_switch(conf)
        await cg.register_component(sw, conf)
        await cg.register_parented(sw, hub)
        cg.add(hub.set_power_switch(sw))
