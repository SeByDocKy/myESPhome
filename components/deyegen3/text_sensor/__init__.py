import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from .. import CONF_DEYEGEN3_ID, DeyeGen3Component

DEPENDENCIES = ["deyegen3"]

CONF_INVERTER_STATUS = "inverter_status"

# Unlike tsungen3 (raw hex + separately decoded alarm/fault bitmasks),
# inverter_status here is already a small enum decoded straight to a string
# ("Stand-by"/"Self-check"/"Normal"/"Warning"/"Fault") -- no separate
# event_alarms/event_faults text_sensor exists for this protocol, since no
# equivalent bitmask register was found in the source register map.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYEGEN3_ID): cv.use_id(DeyeGen3Component),
        cv.Optional(CONF_INVERTER_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:information-outline",
        ),
    }
)

_SETTERS = {
    CONF_INVERTER_STATUS: "set_inverter_status_text_sensor",
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DEYEGEN3_ID])
    for key, setter in _SETTERS.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(hub, setter)(sens))
