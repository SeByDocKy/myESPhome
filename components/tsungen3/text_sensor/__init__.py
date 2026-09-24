import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from .. import CONF_TSUNGEN3_ID, TSunGen3Component

DEPENDENCIES = ["tsungen3"]

CONF_INVERTER_STATUS = "inverter_status"
CONF_EVENT_ALARMS = "event_alarms"
CONF_EVENT_FAULTS = "event_faults"

ICON_INVERTER_STATUS = "mdi:information-outline"
ICON_EVENT_ALARMS = "mdi:alert-outline"
ICON_EVENT_FAULTS = "mdi:alert-circle-outline"

# v1 scope: the Inverter Status / Event Alarms / Event Faults registers are
# published as raw "0x____" hex strings. The bitmap meanings are not documented
# upstream (s-allius/tsun-gen3-proxy exposes them as opaque integers too) --
# decoding them into named states is left for a later revision once real capture
# data is available, same incremental approach as this author's hm/hms components.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSUNGEN3_ID): cv.use_id(TSunGen3Component),
        cv.Optional(CONF_INVERTER_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_INVERTER_STATUS
        ),
        cv.Optional(CONF_EVENT_ALARMS): text_sensor.text_sensor_schema(
            icon=ICON_EVENT_ALARMS
        ),
        cv.Optional(CONF_EVENT_FAULTS): text_sensor.text_sensor_schema(
            icon=ICON_EVENT_FAULTS
        ),
    }
)

_SETTERS = {
    CONF_INVERTER_STATUS: "set_inverter_status_text_sensor",
    CONF_EVENT_ALARMS: "set_event_alarms_text_sensor",
    CONF_EVENT_FAULTS: "set_event_faults_text_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_TSUNGEN3_ID])
    for key, setter in _SETTERS.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(hub, setter)(sens))
