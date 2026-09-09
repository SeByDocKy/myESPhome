import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import packet_transport

from .. import NRF24Component, nrf24l01_ns

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["nrf24l01"]

CONF_NRF24L01_ID = "nrf24l01_id"

NRF24L01Transport = nrf24l01_ns.class_(
    "NRF24L01Transport",
    packet_transport.PacketTransport,
    cg.Parented.template(NRF24Component),
)

CONFIG_SCHEMA = packet_transport.transport_schema(NRF24L01Transport).extend(
    {
        cv.GenerateID(CONF_NRF24L01_ID): cv.use_id(NRF24Component),
    }
)


async def to_code(config):
    var, _providers = await packet_transport.new_packet_transport(config)
    await cg.register_parented(var, config[CONF_NRF24L01_ID])
