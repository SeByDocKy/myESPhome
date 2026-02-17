import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)

# Import from parent rylr998 component
from .. import CONF_RYLR998_ID, RYLR998Component, rylr998_ns

DEPENDENCIES = ["rylr998"]

# PacketTransport already inherits from PollingComponent
RYLR998Transport = rylr998_ns.class_(
    "RYLR998Transport", 
    PacketTransport,
)

CONFIG_SCHEMA = transport_schema(RYLR998Transport).extend(
    {
        cv.Required(CONF_RYLR998_ID): cv.use_id(RYLR998Component),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    rylr998 = await cg.get_variable(config[CONF_RYLR998_ID])
    cg.add(var.set_parent(rylr998))
