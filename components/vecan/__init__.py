import esphome.codegen as cg
from esphome.components import canbus
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = []
MULTI_CONF = True

CONF_CANBUS_ID = "canbus_id"
CONF_ADDRESS = "address"
CONF_LISTEN_ONLY = "listen_only"
CONF_TX_INTERVAL = "tx_interval"

vecan_ns = cg.esphome_ns.namespace("vecan")
VeCanHub = vecan_ns.class_("VeCanHub", cg.Component)
VeCanDevice = vecan_ns.class_("VeCanDevice")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VeCanHub),
        cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        # Source address we try to claim on the bus (ISO 11783-5 address claim, 128..247 recommended)
        cv.Optional(CONF_ADDRESS, default=0xA0): cv.int_range(min=0, max=247),
        # Never transmit anything (no address claim, no requests): passive sniffing of the bus
        cv.Optional(CONF_LISTEN_ONLY, default=False): cv.boolean,
        # Minimum spacing between two transmitted frames
        cv.Optional(
            CONF_TX_INTERVAL, default="20ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(parent))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_listen_only(config[CONF_LISTEN_ONLY]))
    cg.add(var.set_tx_interval(config[CONF_TX_INTERVAL]))
