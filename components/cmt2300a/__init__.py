import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@SeByDocKy"]

cmt2300a_ns = cg.esphome_ns.namespace("cmt2300a")
CMT2300AComponent = cmt2300a_ns.class_("CMT2300AComponent", cg.Component)

CMT2300ASendAction = cmt2300a_ns.class_("CMT2300ASendAction", automation.Action)

CMT2300APacketReceivedTrigger = cmt2300a_ns.class_(
    "CMT2300APacketReceivedTrigger", automation.Trigger.template(cg.std_vector.template(cg.uint8))
)
CMT2300ATxDoneTrigger = cmt2300a_ns.class_(
    "CMT2300ATxDoneTrigger", automation.Trigger.template()
)

CONF_CLK_PIN = "clk_pin"
CONF_SDIO_PIN = "sdio_pin"
CONF_CS_PIN = "cs_pin"
CONF_FCS_PIN = "fcs_pin"
CONF_GPIO2_PIN = "gpio2_pin"
CONF_GPIO3_PIN = "gpio3_pin"
CONF_NODE_ID = "node_id"
CONF_ACCEPT_ANY_NODE_ID = "accept_any_node_id"
CONF_FIFO_THRESHOLD = "fifo_threshold"
CONF_ON_PACKET_RECEIVED = "on_packet_received"
CONF_ON_TX_DONE = "on_tx_done"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CMT2300AComponent),
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_SDIO_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_FCS_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_GPIO2_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_GPIO3_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_NODE_ID, default=0): cv.uint32_t,
        cv.Optional(CONF_ACCEPT_ANY_NODE_ID, default=False): cv.boolean,
        cv.Optional(CONF_FIFO_THRESHOLD, default=32): cv.int_range(min=1, max=64),
        cv.Optional(CONF_ON_PACKET_RECEIVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    CMT2300APacketReceivedTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_TX_DONE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CMT2300ATxDoneTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    clk_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk_pin))
    sdio_pin = await cg.gpio_pin_expression(config[CONF_SDIO_PIN])
    cg.add(var.set_sdio_pin(sdio_pin))
    cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs_pin))
    fcs_pin = await cg.gpio_pin_expression(config[CONF_FCS_PIN])
    cg.add(var.set_fcs_pin(fcs_pin))

    if CONF_GPIO2_PIN in config:
        gpio2_pin = await cg.gpio_pin_expression(config[CONF_GPIO2_PIN])
        cg.add(var.set_gpio2_pin(gpio2_pin))
    if CONF_GPIO3_PIN in config:
        gpio3_pin = await cg.gpio_pin_expression(config[CONF_GPIO3_PIN])
        cg.add(var.set_gpio3_pin(gpio3_pin))

    cg.add(var.set_node_id(config[CONF_NODE_ID]))
    cg.add(var.set_accept_any_node_id(config[CONF_ACCEPT_ANY_NODE_ID]))
    cg.add(var.set_fifo_threshold(config[CONF_FIFO_THRESHOLD]))

    for conf in config.get(CONF_ON_PACKET_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_vector.template(cg.uint8), "x")], conf
        )

    for conf in config.get(CONF_ON_TX_DONE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


CMT2300A_SEND_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(CMT2300AComponent),
        cv.Required("data"): cv.templatable(cv.ensure_list(cv.uint8_t)),
    }
)


@automation.register_action(
    "cmt2300a.send", CMT2300ASendAction, CMT2300A_SEND_ACTION_SCHEMA
)
async def cmt2300a_send_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    data = config["data"]
    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    return var
