import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import packet_transport
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.const import CONF_ID

from .. import CONF_RYLR998_ID, RYLR998Component, rylr998_ns

DEPENDENCIES = ["rylr998"]

RYLR998PacketTransportComponent = rylr998_ns.class_(
    "RYLR998PacketTransportComponent", PacketTransport
)


CONFIG_SCHEMA = transport_schema(RYLR998PacketTransportComponent).extend(
    {
        cv.GenerateID(CONF_RYLR998_ID): cv.use_id(RYLR998Component),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    rylr998 = await cg.get_variable(config[CONF_RYLR998_ID])
    cg.add(var.set_parent(rylr998))

