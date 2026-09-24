import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import HMSWComponent, hmsw_ns

CONF_HMSW_ID = "hmsw_id"
# Reboots the DTU/inverter's own network stack (CMD_ACTION_DTU_REBOOT, sent
# on a distinct wire command 0x23 0x05) -- NOT the ESP32 this component
# runs on. Ported from ohAnd/dtuGateway's requestRestartDevice(); that
# project exposes an equivalent "Reboot DTU" button in its own web UI, and
# also fires it automatically as part of its own hang/error recovery. This
# component only exposes it as a manual, explicit action -- see README.md.
CONF_RESET_HMSW = "reset_hmsw"

DEPENDENCIES = ["hmsw"]

HMSWResetButton = hmsw_ns.class_("HMSWResetButton", button.Button)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMSW_ID): cv.use_id(HMSWComponent),
        cv.Optional(CONF_RESET_HMSW): button.button_schema(
            HMSWResetButton,
            device_class="restart",
            entity_category="diagnostic",
            icon="mdi:restart-alert",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMSW_ID])

    if CONF_RESET_HMSW in config:
        conf = config[CONF_RESET_HMSW]
        b = await button.new_button(conf)
        cg.add(b.set_parent(hub))
