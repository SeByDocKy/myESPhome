"""Plateforme select pour le composant usbuvc.

Crée un select qui se peuple automatiquement avec les résolutions MJPEG
disponibles sur la caméra connectée, et applique le changement de format
via usbuvc.change_format à chaque sélection.

Usage YAML :
  select:
    - platform: usbuvc
      name: "Résolution caméra"
      usbuvc_id: my_webcam
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from .. import usbuvc_ns, UsbUvcCamera

DEPENDENCIES = ["usbuvc"]

CONF_USBUVC_ID = "usbuvc_id"

UvcResolutionSelect = usbuvc_ns.class_(
    "UvcResolutionSelect", select.Select, cg.Component
)

CONFIG_SCHEMA = select.select_schema(UvcResolutionSelect).extend(
    {
        cv.GenerateID(): cv.declare_id(UvcResolutionSelect),
        cv.Required(CONF_USBUVC_ID): cv.use_id(UsbUvcCamera),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Pas d'options statiques — elles seront peuplées dynamiquement au runtime
    await select.register_select(var, config, options=[])

    parent = await cg.get_variable(config[CONF_USBUVC_ID])
    cg.add(parent.set_resolution_select(var))
