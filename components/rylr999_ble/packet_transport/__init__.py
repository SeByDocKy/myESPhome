import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv
from esphome.cpp_types import PollingComponent

from .. import CONF_RYLR999_BLE_ID, RYLR999BLEComponent, RYLR999BLEListener, rylr999_ble_ns

DEPENDENCIES = ["rylr999_ble"]

RYLR999BLETransport = rylr999_ble_ns.class_(
    "RYLR999BLETransport", PacketTransport, PollingComponent, RYLR999BLEListener
)

CONFIG_SCHEMA = transport_schema(RYLR999BLETransport).extend(
    {
        cv.GenerateID(CONF_RYLR999_BLE_ID): cv.use_id(RYLR999BLEComponent),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    parent = await cg.get_variable(config[CONF_RYLR999_BLE_ID])
    cg.add(var.set_parent(parent))
