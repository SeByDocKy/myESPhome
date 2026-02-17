import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["packet_transport", "sensor"]

rylr998_ns = cg.esphome_ns.namespace("rylr998")
RYLR998Component = rylr998_ns.class_(
    "RYLR998Component", cg.Component, uart.UARTDevice
)

CONF_RYLR998_ID = "rylr998_id"
CONF_ADDRESS     = "address"
CONF_NETWORK_ID  = "network_id"
CONF_BAND        = "band"
CONF_SF          = "sf"
CONF_BW          = "bw"
CONF_CR          = "cr"
CONF_PREAMBLE    = "preamble"
CONF_PASSWORD    = "password"
CONF_OUTPUT_POWER = "output_power"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RYLR998Component),
            cv.Optional(CONF_ADDRESS, default=0):      cv.int_range(min=0, max=65535),
            cv.Optional(CONF_NETWORK_ID, default=18):  cv.int_range(min=0, max=255),
            cv.Optional(CONF_BAND, default=868000000): cv.int_range(min=820000000, max=960000000),
            cv.Optional(CONF_SF, default=9):           cv.int_range(min=7, max=12),
            cv.Optional(CONF_BW, default=7):           cv.int_range(min=7, max=9),
            cv.Optional(CONF_CR, default=1):           cv.int_range(min=1, max=4),
            cv.Optional(CONF_PREAMBLE, default=12):    cv.int_range(min=4, max=25),
            cv.Optional(CONF_PASSWORD, default="FABC0002EEDCAA90"):
                cv.string,
            cv.Optional(CONF_OUTPUT_POWER, default=22): cv.int_range(min=0, max=22),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_network_id(config[CONF_NETWORK_ID]))
    cg.add(var.set_band(config[CONF_BAND]))
    cg.add(var.set_sf(config[CONF_SF]))
    cg.add(var.set_bw(config[CONF_BW]))
    cg.add(var.set_cr(config[CONF_CR]))
    cg.add(var.set_preamble(config[CONF_PREAMBLE]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_output_power(config[CONF_OUTPUT_POWER]))
