import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from .. import CONF_FAIKIN_ID, FaikinComponent, faikin_ns

DEPENDENCIES = ["faikin"]

FaikinSwitch = faikin_ns.class_(
    "FaikinSwitch", switch.Switch, cg.Parented.template(FaikinComponent)
)
Flag = faikin_ns.enum("Flag", is_class=True)

CONF_ECONO = "econo"
CONF_POWERFUL = "powerful"
CONF_COMFORT = "comfort"
CONF_QUIET = "quiet"
CONF_STREAMER = "streamer"
CONF_SENSOR = "sensor"

# key -> (flag, icon)
SWITCHES = {
    CONF_ECONO: (Flag.ECONO, "mdi:leaf"),
    CONF_POWERFUL: (Flag.POWERFUL, "mdi:flash"),
    CONF_COMFORT: (Flag.COMFORT, "mdi:sofa"),
    # Outdoor unit quiet mode.
    CONF_QUIET: (Flag.QUIET, "mdi:volume-low"),
    CONF_STREAMER: (Flag.STREAMER, "mdi:air-filter"),
    CONF_SENSOR: (Flag.SENSOR, "mdi:motion-sensor"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FAIKIN_ID): cv.use_id(FaikinComponent),
        **{
            cv.Optional(key): switch.switch_schema(FaikinSwitch, icon=icon)
            for key, (_, icon) in SWITCHES.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FAIKIN_ID])
    for key, (flag, _) in SWITCHES.items():
        if conf := config.get(key):
            var = await switch.new_switch(conf)
            cg.add(var.set_parent(parent))
            cg.add(var.set_flag(flag))
            cg.add(getattr(parent, f"set_{key}_switch")(var))
