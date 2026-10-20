"""ESPHome native component emulating the RIKA Firenet 2.0 USB dongle (open-firenet protocol)."""

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_OPENFIRENET_ID = "openfirenet_id"
CONF_FRAME_GAP = "frame_gap"

openfirenet_ns = cg.esphome_ns.namespace("openfirenet")
OpenFireNetComponent = openfirenet_ns.class_(
    "OpenFireNetComponent", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenFireNetComponent),
            # Silence between two dongle -> stove frames. The stove firmware needs a pause to
            # parse one frame before it receives the next.
            cv.Optional(CONF_FRAME_GAP, default="100ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=20), max=cv.TimePeriod(seconds=2)),
            ),
        }
    )
    .extend(cv.polling_component_schema("2s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_frame_gap(config[CONF_FRAME_GAP]))
