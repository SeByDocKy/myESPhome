import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import HMSFwUpdateComponent, hms_fwupdate_ns

CONF_HMS_FWUPDATE_ID = "hms_fwupdate_id"
CONF_ARMED = "armed"

DEPENDENCIES = ["hms_fwupdate"]

HMSFwUpdateArmedSwitch = hms_fwupdate_ns.class_("HMSFwUpdateArmedSwitch", switch.Switch)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_FWUPDATE_ID): cv.use_id(HMSFwUpdateComponent),
        # Safety interlock: turn it on, then press `start` within `armed_timeout`.
        # It turns itself off when the timeout expires or when an update starts.
        cv.Optional(CONF_ARMED): switch.switch_schema(
            HMSFwUpdateArmedSwitch,
            icon="mdi:shield-lock-open-outline",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_FWUPDATE_ID])

    if CONF_ARMED in config:
        s = await switch.new_switch(config[CONF_ARMED])
        cg.add(s.set_parent(hub))
        cg.add(hub.set_armed_switch(s))
