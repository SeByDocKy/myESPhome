import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)

# Import from parent rylr998 component
from .. import RYLR998Component, rylr998_ns

DEPENDENCIES = ["rylr998"]

CONF_RYLR998_ID = "rylr998_id"

# PollingComponent is imported via cg
PollingComponent = cg.PollingComponent

RYLR998Transport = rylr998_ns.class_(
    "RYLR998Transport", 
    PacketTransport, 
    PollingComponent,
)

CONFIG_SCHEMA = transport_schema(RYLR998Transport).extend(
    {
        cv.GenerateID(CONF_RYLR998_ID): cv.use_id(RYLR998Component),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    rylr998 = await cg.get_variable(config[CONF_RYLR998_ID])
    cg.add(var.set_parent(rylr998))
