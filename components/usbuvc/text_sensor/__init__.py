"""Plateforme text_sensor pour le composant usbuvc.

Déclare un text_sensor qui se peuple automatiquement avec la liste
des formats MJPEG disponibles sur la caméra connectée.

Usage YAML :
  text_sensor:
    - platform: usbuvc
      name: "Formats caméra"
      usbuvc_id: my_webcam
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import usbuvc_ns, UsbUvcCamera

DEPENDENCIES = ["usbuvc"]

CONF_USBUVC_ID = "usbuvc_id"

UvcFormatListSensor = usbuvc_ns.class_(
    "UvcFormatListSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(UvcFormatListSensor).extend(
    {
        cv.GenerateID(): cv.declare_id(UvcFormatListSensor),
        cv.Required(CONF_USBUVC_ID): cv.use_id(UsbUvcCamera),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    parent = await cg.get_variable(config[CONF_USBUVC_ID])
    cg.add(parent.set_format_list_sensor(var))
