"""Native ESPHome client for Daikin air conditioners (S21 serial port).

Phase 1 covers the S21 protocol only. The protocol layer is a port of the reverse engineering work
done in ESP32-Faikin / ESP32-Faikout.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL
from esphome.core import TimePeriod

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["uart"]

CONF_FAIKIN_ID = "faikin_id"
CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_COMMAND_GAP = "command_gap"
CONF_PROBE = "probe"
CONF_COMMAND = "command"
CONF_PAYLOAD = "payload"

faikin_ns = cg.esphome_ns.namespace("faikin")
FaikinComponent = faikin_ns.class_("FaikinComponent", cg.Component, uart.UARTDevice)
SendRawAction = faikin_ns.class_("SendRawAction", automation.Action)
ProbeAction = faikin_ns.class_("ProbeAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FaikinComponent),
            cv.Optional(CONF_UPDATE_INTERVAL, default="1s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=TimePeriod(milliseconds=200)),
            ),
            cv.Optional(CONF_RESPONSE_TIMEOUT, default="500ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=TimePeriod(milliseconds=100), max=TimePeriod(milliseconds=5000)
                ),
            ),
            cv.Optional(CONF_COMMAND_GAP, default="25ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=TimePeriod(milliseconds=1000)),
            ),
            cv.Optional(CONF_PROBE, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

# The S21 port is 2400 baud, 8 data bits, even parity, 2 stop bits. The Tx / Rx pin inversion that some
# boards need is configured on the uart pins themselves (`inverted: true`).
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "faikin",
    baud_rate=2400,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="EVEN",
    stop_bits=2,
)

SEND_RAW_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(FaikinComponent),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
        cv.Optional(CONF_PAYLOAD, default=""): cv.templatable(cv.string),
    }
)

PROBE_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(FaikinComponent)})


@automation.register_action(
    "faikin.send_raw", SendRawAction, SEND_RAW_SCHEMA, synchronous=True
)
async def send_raw_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    command = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(command))
    payload = await cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(payload))
    return var


@automation.register_action(
    "faikin.probe", ProbeAction, PROBE_SCHEMA, synchronous=True
)
async def probe_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # update_interval is applied by register_component() through set_update_interval().
    cg.add(var.set_response_timeout_ms(config[CONF_RESPONSE_TIMEOUT]))
    cg.add(var.set_command_gap_ms(config[CONF_COMMAND_GAP]))
    cg.add(var.set_probe(config[CONF_PROBE]))
