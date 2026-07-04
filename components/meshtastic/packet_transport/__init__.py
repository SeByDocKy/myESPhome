import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv
from esphome.cpp_types import PollingComponent

from .. import CONF_MESHTASTIC_ID, Meshtastic, meshtastic_ns, CONF_HOPLIMIT

CODEOWNERS = ["@Andrik45719"]


MeshtasticTransport = meshtastic_ns.class_(
    "MeshtasticTransport", PacketTransport, PollingComponent
)

CONFIG_SCHEMA = transport_schema(MeshtasticTransport).extend(
    {
        cv.GenerateID(CONF_MESHTASTIC_ID): cv.use_id(Meshtastic),
        cv.Optional(CONF_HOPLIMIT, 0): cv.int_range(0, 7),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    meshtastic = await cg.get_variable(config[CONF_MESHTASTIC_ID])
    cg.add(var.set_parent(meshtastic))
    cg.add(var.set_hop_limit(config[CONF_HOPLIMIT]))
