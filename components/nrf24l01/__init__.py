import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.components import spi
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_CHANNEL

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["spi"]

nrf24l01_ns = cg.esphome_ns.namespace("nrf24l01")
NRF24Component = nrf24l01_ns.class_("NRF24Component", cg.Component, spi.SPIDevice)

NRF24SendAction = nrf24l01_ns.class_("NRF24SendAction", automation.Action)
NRF24PacketReceivedTrigger = nrf24l01_ns.class_(
    "NRF24PacketReceivedTrigger", automation.Trigger.template(cg.std_vector.template(cg.uint8))
)

CONF_CE_PIN = "ce_pin"
CONF_IRQ_PIN = "irq_pin"
CONF_PA_LEVEL = "pa_level"
CONF_DATA_RATE = "data_rate"
CONF_CRC_LENGTH = "crc_length"
CONF_ADDRESS_WIDTH = "address_width"
CONF_AUTO_ACK = "auto_ack"
CONF_RETRY_DELAY = "retry_delay"
CONF_RETRY_COUNT = "retry_count"
CONF_PAYLOAD_SIZE = "payload_size"
CONF_DYNAMIC_PAYLOADS = "dynamic_payloads"
CONF_TX_ADDRESS = "tx_address"
CONF_RX_ADDRESS = "rx_address"
CONF_ON_PACKET_RECEIVED = "on_packet_received"

PA_LEVELS = {"min": 0, "low": 1, "high": 2, "max": 3}
DATA_RATES = {"1mbps": 0, "2mbps": 1, "250kbps": 2}
CRC_LENGTHS = {"disabled": 0, "8bit": 1, "16bit": 2}


def _validate_address(value):
    value = cv.string_strict(value)
    if len(value) != 10 or not all(c in "0123456789abcdefABCDEF" for c in value):
        raise cv.Invalid(
            "Address must be exactly 10 hex characters (5 bytes), e.g. 'E7E7E7E7E7'"
        )
    return value.upper()


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NRF24Component),
            cv.Required(CONF_CE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_CHANNEL, default=76): cv.int_range(min=0, max=125),
            cv.Optional(CONF_PA_LEVEL, default="max"): cv.enum(PA_LEVELS, lower=True),
            cv.Optional(CONF_DATA_RATE, default="1mbps"): cv.enum(
                DATA_RATES, lower=True
            ),
            cv.Optional(CONF_CRC_LENGTH, default="16bit"): cv.enum(
                CRC_LENGTHS, lower=True
            ),
            cv.Optional(CONF_ADDRESS_WIDTH, default=5): cv.int_range(min=3, max=5),
            cv.Optional(CONF_AUTO_ACK, default=True): cv.boolean,
            cv.Optional(CONF_RETRY_DELAY, default=5): cv.int_range(min=0, max=15),
            cv.Optional(CONF_RETRY_COUNT, default=15): cv.int_range(min=0, max=15),
            cv.Optional(CONF_PAYLOAD_SIZE, default=32): cv.int_range(min=1, max=32),
            cv.Optional(CONF_DYNAMIC_PAYLOADS, default=False): cv.boolean,
            cv.Optional(CONF_TX_ADDRESS, default="E7E7E7E7E7"): _validate_address,
            cv.Optional(CONF_RX_ADDRESS, default="E7E7E7E7E7"): _validate_address,
            cv.Optional(CONF_ON_PACKET_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        NRF24PacketReceivedTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    ce_pin = await cg.gpio_pin_expression(config[CONF_CE_PIN])
    cg.add(var.set_ce_pin(ce_pin))
    if CONF_IRQ_PIN in config:
        irq_pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
        cg.add(var.set_irq_pin(irq_pin))

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_pa_level(config[CONF_PA_LEVEL]))
    cg.add(var.set_data_rate(config[CONF_DATA_RATE]))
    cg.add(var.set_crc_length(config[CONF_CRC_LENGTH]))
    cg.add(var.set_address_width(config[CONF_ADDRESS_WIDTH]))
    cg.add(var.set_auto_ack(config[CONF_AUTO_ACK]))
    cg.add(var.set_retry_delay(config[CONF_RETRY_DELAY]))
    cg.add(var.set_retry_count(config[CONF_RETRY_COUNT]))
    cg.add(var.set_payload_size(config[CONF_PAYLOAD_SIZE]))
    cg.add(var.set_dynamic_payloads(config[CONF_DYNAMIC_PAYLOADS]))

    tx_addr = [int(config[CONF_TX_ADDRESS][i : i + 2], 16) for i in range(0, 10, 2)]
    cg.add(var.set_tx_address(tx_addr))
    rx_addr = [int(config[CONF_RX_ADDRESS][i : i + 2], 16) for i in range(0, 10, 2)]
    cg.add(var.set_rx_address(rx_addr))

    for conf in config.get(CONF_ON_PACKET_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_vector.template(cg.uint8), "x")], conf
        )


NRF24_SEND_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(NRF24Component),
        cv.Required("data"): cv.templatable(cv.ensure_list(cv.uint8_t)),
    }
)


@automation.register_action(
    "nrf24l01.send", NRF24SendAction, NRF24_SEND_ACTION_SCHEMA
)
async def nrf24l01_send_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    data = config["data"]
    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    return var
