import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from .. import HMSFwUpdateComponent

CONF_HMS_FWUPDATE_ID = "hms_fwupdate_id"
CONF_STATE = "state"
CONF_RESULT = "result"

DEPENDENCIES = ["hms_fwupdate"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_FWUPDATE_ID): cv.use_id(HMSFwUpdateComponent),
        # idle / scanning / starting / sending / done / dry_run_done / failed / aborted
        cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(
            icon="mdi:state-machine",
        ),
        # "<outcome>: <detail>", e.g. "failed: link quality too low (70% < 90%)"
        cv.Optional(CONF_RESULT): text_sensor.text_sensor_schema(
            icon="mdi:clipboard-text-outline",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_FWUPDATE_ID])

    if CONF_STATE in config:
        s = await text_sensor.new_text_sensor(config[CONF_STATE])
        cg.add(hub.set_state_text_sensor(s))

    if CONF_RESULT in config:
        s = await text_sensor.new_text_sensor(config[CONF_RESULT])
        cg.add(hub.set_result_text_sensor(s))
