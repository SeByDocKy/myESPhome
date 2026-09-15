import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from .. import ez1m_ns, EZ1MComponent, CONF_EZ1M_ID

DEPENDENCIES = ["ez1m"]

EZ1MTextSensor = ez1m_ns.class_(
    "EZ1MTextSensor", text_sensor.TextSensor, cg.Parented.template(EZ1MComponent)
)
EZ1MTextSensorType = ez1m_ns.enum("EZ1MTextSensorType", is_class=True)

CONF_INVERTER_STATE = "inverter_state"
CONF_DSP_VERSION = "dsp_version"

TEXT_SENSOR_TYPES = {
    CONF_INVERTER_STATE: text_sensor.text_sensor_schema(EZ1MTextSensor, icon="mdi:state-machine"),
    CONF_DSP_VERSION: text_sensor.text_sensor_schema(
        EZ1MTextSensor, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EZ1M_ID): cv.use_id(EZ1MComponent),
        **{cv.Optional(key): schema for key, schema in TEXT_SENSOR_TYPES.items()},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EZ1M_ID])
    for key in TEXT_SENSOR_TYPES:
        if key not in config:
            continue
        conf = config[key]
        sens = await text_sensor.new_text_sensor(conf)
        await cg.register_parented(sens, hub)
        cg.add(hub.register_text_sensor(sens, getattr(EZ1MTextSensorType, key.upper())))
