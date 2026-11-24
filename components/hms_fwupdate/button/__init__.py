import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import HMSFwUpdateComponent, hms_fwupdate_ns

CONF_HMS_FWUPDATE_ID = "hms_fwupdate_id"
CONF_START = "start"
CONF_ABORT = "abort"

DEPENDENCIES = ["hms_fwupdate"]

HMSFwUpdateStartButton = hms_fwupdate_ns.class_("HMSFwUpdateStartButton", button.Button)
HMSFwUpdateAbortButton = hms_fwupdate_ns.class_("HMSFwUpdateAbortButton", button.Button)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_FWUPDATE_ID): cv.use_id(HMSFwUpdateComponent),
        # Downloads, verifies and (unless dry_run) sends the selected firmware.
        cv.Optional(CONF_START): button.button_schema(
            HMSFwUpdateStartButton,
            icon="mdi:upload",
        ),
        cv.Optional(CONF_ABORT): button.button_schema(
            HMSFwUpdateAbortButton,
            icon="mdi:cancel",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_FWUPDATE_ID])

    if CONF_START in config:
        b = await button.new_button(config[CONF_START])
        cg.add(b.set_parent(hub))

    if CONF_ABORT in config:
        b = await button.new_button(config[CONF_ABORT])
        cg.add(b.set_parent(hub))
