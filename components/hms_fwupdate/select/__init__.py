import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_NAME
from esphome.core import CORE

from .. import CONF_FIRMWARES, HMSFwUpdateComponent, hms_fwupdate_ns

CONF_HMS_FWUPDATE_ID = "hms_fwupdate_id"
CONF_FIRMWARE = "firmware"

DEPENDENCIES = ["hms_fwupdate"]

HMSFwUpdateFirmwareSelect = hms_fwupdate_ns.class_("HMSFwUpdateFirmwareSelect", select.Select)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_FWUPDATE_ID): cv.use_id(HMSFwUpdateComponent),
        # The options are the `name` of each entry of the hub's `firmwares:` list.
        cv.Optional(CONF_FIRMWARE): select.select_schema(
            HMSFwUpdateFirmwareSelect,
            icon="mdi:file-cog-outline",
        ),
    }
)


def _firmware_names(hub_id):
    for conf in CORE.config.get("hms_fwupdate", []):
        if conf[CONF_ID].id == hub_id.id:
            return [fw[CONF_NAME] for fw in conf[CONF_FIRMWARES]]
    raise cv.Invalid(f"hms_fwupdate '{hub_id.id}' not found")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_FWUPDATE_ID])

    if CONF_FIRMWARE in config:
        options = _firmware_names(config[CONF_HMS_FWUPDATE_ID])
        s = await select.new_select(config[CONF_FIRMWARE], options=options)
        cg.add(s.set_parent(hub))
        cg.add(hub.set_firmware_select(s))
