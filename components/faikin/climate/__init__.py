import esphome.codegen as cg
from esphome.components import climate
import esphome.config_validation as cv

from .. import CONF_FAIKIN_ID, FaikinComponent, faikin_ns

DEPENDENCIES = ["faikin"]

FaikinClimate = faikin_ns.class_(
    "FaikinClimate",
    climate.Climate,
    cg.Component,
    cg.Parented.template(FaikinComponent),
)

CONFIG_SCHEMA = (
    climate.climate_schema(FaikinClimate)
    .extend(
        {
            cv.GenerateID(CONF_FAIKIN_ID): cv.use_id(FaikinComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_FAIKIN_ID])
    cg.add(var.set_parent(paren))
