import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_PCM3K6W_ID, PCM3K6W_PLATFORM_SCHEMA, pcm3k6w_ns

DEPENDENCIES = ["canbus"]

PCM3K6WTextSensor = pcm3k6w_ns.class_("PCM3K6WTextSensor", text_sensor.TextSensor)

# Kind indices MUST match the TextSensorKind enum order in pcm3k6w.h.
KIND_PROGRAM_VERSION, KIND_SN_CODE, KIND_RUNNING_MODE, KIND_OFFGRID_FREQUENCY = range(4)

# (config key, kind, icon)
TEXT_SENSORS = [
    ("program_version", KIND_PROGRAM_VERSION, "mdi:information"),
    ("sn_code", KIND_SN_CODE, "mdi:barcode"),
    ("running_mode", KIND_RUNNING_MODE, "mdi:state-machine"),
    ("inverter_frequency", KIND_OFFGRID_FREQUENCY, "mdi:metronome"),
]

CONFIG_SCHEMA = PCM3K6W_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): text_sensor.text_sensor_schema(
            PCM3K6WTextSensor,
            icon=icon,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        )
        for key, _kind, icon in TEXT_SENSORS
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PCM3K6W_ID])
    for key, kind, _icon in TEXT_SENSORS:
        if key in config:
            s = await text_sensor.new_text_sensor(config[key])
            cg.add(hub.set_text_sensor(kind, s))
