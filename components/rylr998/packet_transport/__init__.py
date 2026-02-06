import esphome.codegen as cg
import esphome.config_validation as cv
# from esphome.components import packet_transport
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.const import CONF_ID

# Import from parent rylr998 component
from .. import RYLR998Component, rylr998_ns

DEPENDENCIES = ["rylr998"]

# RYLR998PacketTransportComponent = rylr998_ns.class_(
#     "RYLR998PacketTransportComponent", packet_transport.PacketTransportComponent
# )

RYLR998PacketTransportComponent = rylr998_ns.class_(
    "RYLR998PacketTransportComponent", PacketTransport
)


# CONFIG_SCHEMA = packet_transport.PACKET_TRANSPORT_SCHEMA.extend(
#     {
#         cv.GenerateID(): cv.declare_id(RYLR998PacketTransportComponent),
#         cv.GenerateID(packet_transport.CONF_PACKET_TRANSPORT_ID): cv.use_id(
#             RYLR998Component
#         ),
#     }
# )


CONFIG_SCHEMA = transport_schema(RYLR998PacketTransportComponent).extend(
    {
        cv.GenerateID(CONF_RYLR998_ID): cv.use_id(RYLR998Component),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await packet_transport.register_packet_transport(var, config)
    
    parent = await cg.get_variable(config[packet_transport.CONF_PACKET_TRANSPORT_ID])
    cg.add(var.set_parent(parent))
