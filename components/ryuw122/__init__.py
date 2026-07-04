import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome import automation
from esphome.const import CONF_ID, CONF_ADDRESS, CONF_MODE, CONF_CHANNEL, CONF_TRIGGER_ID

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

ryuw122_ns = cg.esphome_ns.namespace("ryuw122")
RYUW122Component = ryuw122_ns.class_(
    "RYUW122Component", cg.Component, uart.UARTDevice
)

RYUW122AnchorRcvTrigger = ryuw122_ns.class_(
    "RYUW122AnchorRcvTrigger", automation.Trigger.template()
)
RYUW122TagRcvTrigger = ryuw122_ns.class_(
    "RYUW122TagRcvTrigger", automation.Trigger.template()
)

RYUW122TagSendAction = ryuw122_ns.class_("RYUW122TagSendAction", automation.Action)
RYUW122AnchorSendAction = ryuw122_ns.class_(
    "RYUW122AnchorSendAction", automation.Action
)

CONF_NETWORK_ID = "network_id"
CONF_PASSWORD = "password"
CONF_BANDWIDTH = "bandwidth"
CONF_TX_POWER = "tx_power"
CONF_CALIBRATION = "calibration"
CONF_RSSI_ENABLE = "rssi_enable"
CONF_TAG_RF_ENABLE_MS = "tag_rf_enable_ms"
CONF_TAG_RF_DISABLE_MS = "tag_rf_disable_ms"
CONF_ON_ANCHOR_RCV = "on_anchor_rcv"
CONF_ON_TAG_RCV = "on_tag_rcv"
CONF_TAG_ADDRESS = "tag_address"
CONF_PAYLOAD = "payload"

MODES = {
    "TAG": 0,
    "ANCHOR": 1,
    "SLEEP": 2,
}

BANDWIDTHS = {
    "850KBPS": 0,
    "6.8MBPS": 1,
}

CHANNELS = {
    5: 5,
    9: 9,
}


def validate_8_byte_ascii(value):
    value = cv.string_strict(value)
    if len(value) == 0 or len(value) > 8:
        raise cv.Invalid("must be between 1 and 8 ASCII characters")
    if not value.isascii():
        raise cv.Invalid("must be ASCII only")
    return value


def validate_cpin(value):
    value = cv.string_strict(value)
    if len(value) != 32:
        raise cv.Invalid("password (AT+CPIN) must be exactly 32 hex characters")
    try:
        int(value, 16)
    except ValueError as exc:
        raise cv.Invalid("password (AT+CPIN) must be hexadecimal") from exc
    return value.upper()


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RYUW122Component),
            cv.Optional(CONF_MODE, default="TAG"): cv.enum(MODES, upper=True),
            cv.Optional(CONF_NETWORK_ID, default="REYAX123"): validate_8_byte_ascii,
            cv.Optional(CONF_ADDRESS, default="DAVID123"): validate_8_byte_ascii,
            cv.Optional(CONF_PASSWORD): cv.sensitive(validate_cpin),
            cv.Optional(CONF_CHANNEL, default=5): cv.enum(CHANNELS, int=True),
            cv.Optional(CONF_BANDWIDTH, default="850KBPS"): cv.enum(
                BANDWIDTHS, upper=True
            ),
            cv.Optional(CONF_TX_POWER, default=5): cv.int_range(min=0, max=5),
            cv.Optional(CONF_CALIBRATION, default=0): cv.int_range(
                min=-100, max=100
            ),
            cv.Optional(CONF_RSSI_ENABLE, default=True): cv.boolean,
            cv.Optional(CONF_TAG_RF_ENABLE_MS, default=0): cv.int_range(
                min=0, max=28000
            ),
            cv.Optional(CONF_TAG_RF_DISABLE_MS, default=0): cv.int_range(
                min=0, max=28000
            ),
            cv.Optional(CONF_ON_ANCHOR_RCV): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        RYUW122AnchorRcvTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_TAG_RCV): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        RYUW122TagRcvTrigger
                    ),
                }
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_network_id(config[CONF_NETWORK_ID]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_bandwidth(config[CONF_BANDWIDTH]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))
    cg.add(var.set_calibration(config[CONF_CALIBRATION]))
    cg.add(var.set_rssi_enable(config[CONF_RSSI_ENABLE]))
    cg.add(
        var.set_tag_duty_cycle(
            config[CONF_TAG_RF_ENABLE_MS], config[CONF_TAG_RF_DISABLE_MS]
        )
    )

    for conf in config.get(CONF_ON_ANCHOR_RCV, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (cg.std_string, "tag_address"),
                (cg.std_string, "payload"),
                (cg.float_, "distance"),
                (cg.int_, "rssi"),
            ],
            conf,
        )

    for conf in config.get(CONF_ON_TAG_RCV, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (cg.std_string, "payload"),
                (cg.int_, "rssi"),
            ],
            conf,
        )


RYUW122_TAG_SEND_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RYUW122Component),
        cv.Required(CONF_PAYLOAD): cv.templatable(cv.string_strict),
    }
)


@automation.register_action(
    "ryuw122.tag_send", RYUW122TagSendAction, RYUW122_TAG_SEND_SCHEMA, synchronous=True
)
async def ryuw122_tag_send_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    paren = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(paren))
    template_ = await cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(template_))
    return var


RYUW122_ANCHOR_SEND_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RYUW122Component),
        cv.Required(CONF_TAG_ADDRESS): cv.templatable(validate_8_byte_ascii),
        cv.Required(CONF_PAYLOAD): cv.templatable(cv.string_strict),
    }
)


@automation.register_action(
    "ryuw122.anchor_send",
    RYUW122AnchorSendAction,
    RYUW122_ANCHOR_SEND_SCHEMA,
    synchronous=True,
)
async def ryuw122_anchor_send_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    paren = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(paren))
    template_address = await cg.templatable(
        config[CONF_TAG_ADDRESS], args, cg.std_string
    )
    cg.add(var.set_tag_address(template_address))
    template_payload = await cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(template_payload))
    return var
