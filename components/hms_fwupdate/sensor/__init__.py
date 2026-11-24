import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from .. import HMSFwUpdateComponent

CONF_HMS_FWUPDATE_ID = "hms_fwupdate_id"
CONF_PROGRESS = "progress"
CONF_LINK_QUALITY = "link_quality"

DEPENDENCIES = ["hms_fwupdate"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_FWUPDATE_ID): cv.use_id(HMSFwUpdateComponent),
        # Progress of the running update, in % of the rows acknowledged by the inverter.
        cv.Optional(CONF_PROGRESS): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:progress-upload",
        ),
        # Share of successful telemetry polls over `link_quality_window` polls (what the
        # update checks against `min_link_quality` before it starts).
        cv.Optional(CONF_LINK_QUALITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:signal",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_FWUPDATE_ID])

    if CONF_PROGRESS in config:
        s = await sensor.new_sensor(config[CONF_PROGRESS])
        cg.add(hub.set_progress_sensor(s))

    if CONF_LINK_QUALITY in config:
        s = await sensor.new_sensor(config[CONF_LINK_QUALITY])
        cg.add(hub.set_link_quality_sensor(s))
