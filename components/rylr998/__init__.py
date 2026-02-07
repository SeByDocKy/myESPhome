import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
)

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@yourusername"]

CONF_ADDRESS = "address"
CONF_SPREADING_FACTOR = "spreading_factor"
CONF_SIGNAL_BANDWIDTH = "signal_bandwidth"
CONF_CODING_RATE = "coding_rate"
CONF_PREAMBLE_LENGTH = "preamble_length"
CONF_NETWORK_ID = "network_id"
CONF_TX_POWER = "tx_power"
CONF_ON_PACKET = "on_packet"
CONF_DESTINATION = "destination"
CONF_DATA = "data"

rylr998_ns = cg.esphome_ns.namespace("rylr998")
RYLR998Component = rylr998_ns.class_(
    "RYLR998Component", cg.Component, uart.UARTDevice
)

RYLR998SendPacketAction = rylr998_ns.class_("RYLR998SendPacketAction", automation.Action)

# Use the component's trigger directly, no wrapper needed

# Bandwidth mapping
BANDWIDTH_MAP = {
    125000: 125000,
    250000: 250000,
    500000: 500000,
}

def validate_bandwidth(value):
    value = cv.frequency(value)
    if value not in BANDWIDTH_MAP:
        raise cv.Invalid(f"Bandwidth must be one of {list(BANDWIDTH_MAP.keys())}")
    return value

def validate_spreading_factor(value):
    value = cv.int_(value)
    if value < 5 or value > 11:
        raise cv.Invalid("Spreading factor must be between 5 and 11")
    return value

def validate_coding_rate(value):
    value = cv.int_(value)
    if value < 1 or value > 4:
        raise cv.Invalid("Coding rate must be between 1 and 4")
    return value

def validate_network_id(value):
    value = cv.int_(value)
    valid_ids = list(range(3, 16)) + [18]
    if value not in valid_ids:
        raise cv.Invalid(f"Network ID must be 3-15 or 18")
    return value

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RYLR998Component),
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=65535),
            cv.Optional(CONF_FREQUENCY, default=915000000): cv.frequency,
            cv.Optional(CONF_SPREADING_FACTOR, default=9): validate_spreading_factor,
            cv.Optional(CONF_SIGNAL_BANDWIDTH, default=125000): validate_bandwidth,
            cv.Optional(CONF_CODING_RATE, default=1): validate_coding_rate,
            cv.Optional(CONF_PREAMBLE_LENGTH, default=12): cv.int_range(min=4, max=24),
            cv.Optional(CONF_NETWORK_ID, default=18): validate_network_id,
            cv.Optional(CONF_TX_POWER, default=22): cv.int_range(min=0, max=22),
            cv.Optional(CONF_ON_PACKET): automation.validate_automation(),
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
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_spreading_factor(config[CONF_SPREADING_FACTOR]))
    cg.add(var.set_bandwidth(config[CONF_SIGNAL_BANDWIDTH]))
    cg.add(var.set_coding_rate(config[CONF_CODING_RATE]))
    cg.add(var.set_preamble_length(config[CONF_PREAMBLE_LENGTH]))
    cg.add(var.set_network_id(config[CONF_NETWORK_ID]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))

    for conf in config.get(CONF_ON_PACKET, []):
        trigger = var.Pget_packet_trigger()
        await automation.build_automation(
            trigger,
            [(cg.std_vector.template(cg.uint8), "data"), 
             (cg.float_, "rssi"), (cg.float_, "snr")], 
            conf
        )


# Action for sending data
@automation.register_action(
    "rylr998.send_packet",
    RYLR998SendPacketAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(RYLR998Component),
            cv.Required(CONF_DESTINATION): cv.templatable(cv.int_range(min=0, max=65535)),
            cv.Required(CONF_DATA): cv.templatable(cv.ensure_list(cv.uint8_t)),
        }
    ),
)
async def rylr998_send_packet_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    
    template_ = await cg.templatable(config[CONF_DESTINATION], args, cg.uint16)
    cg.add(var.set_destination(template_))
    
    data = config[CONF_DATA]
    if isinstance(data, cv.Lambda):
        template_ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(template_))
    else:
        cg.add(var.set_data_static(data))
    
    return var
