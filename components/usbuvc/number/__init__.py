"""Plateforme number pour le composant usbuvc.

Expose un slider (1–5) pour contrôler le downsampling_factor à la volée.

Usage YAML :
  number:
    - platform: usbuvc
      name: "Downsampling caméra"
      usbuvc_id: my_webcam
      min_value: 1   # optionnel, défaut 1
      max_value: 5   # optionnel, défaut 5
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP

from .. import usbuvc_ns, UsbUvcCamera

DEPENDENCIES = ["usbuvc"]

CONF_USBUVC_ID = "usbuvc_id"

UvcDownsamplingNumber = usbuvc_ns.class_(
    "UvcDownsamplingNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = number.number_schema(UvcDownsamplingNumber).extend(
    {
        cv.GenerateID(): cv.declare_id(UvcDownsamplingNumber),
        cv.Required(CONF_USBUVC_ID): cv.use_id(UsbUvcCamera),
        cv.Optional(CONF_MIN_VALUE, default=1): cv.int_range(min=1, max=60),
        cv.Optional(CONF_MAX_VALUE, default=5): cv.int_range(min=1, max=60),
        cv.Optional(CONF_STEP, default=1): cv.positive_float,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
    # Force slider mode
    cg.add(var.traits.set_mode(number.number_ns.NUMBER_MODE_SLIDER))

    parent = await cg.get_variable(config[CONF_USBUVC_ID])
    cg.add(parent.set_downsampling_number(var))

    # Publie la valeur initiale = downsampling_factor configuré dans usbuvc:
    # On ne peut pas lire config du parent ici, on publie 1 par défaut
    # L'init correcte se fait dans UvcDownsamplingNumber::setup()
