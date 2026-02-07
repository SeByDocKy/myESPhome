import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import packet_transport
from esphome.const import CONF_ID
from esphome.components.polling_component import PollingComponent

# Import from parent rylr998 component
from .. import RYLR998Component, rylr998_ns

DEPENDENCIES = ["rylr998"]

RYLR998Transport = rylr998_ns.class_(
    "RYLR998Transport", 
    packet_transport.PacketTransport, 
    PollingComponent, 
    # RYLR998Listener is already inherited in C++
)

CONFIG_SCHEMA = packet_transport.packet_transport_schema(RYLR998Transport).extend(
    {
        cv.GenerateID(packet_transport.CONF_PACKET_TRANSPORT_ID): cv.use_id(
            RYLR998Component
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await packet_transport.register_packet_transport(var, config)
    await cg.register_component(var, config)
    
    parent = await cg.get_variable(config[packet_transport.CONF_PACKET_TRANSPORT_ID])
    cg.add(var.set_parent(parent))
