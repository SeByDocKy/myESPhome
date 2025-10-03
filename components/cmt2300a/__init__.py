import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID
from esphome import pins

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@your_github"]

cmt2300a_ns = cg.esphome_ns.namespace("cmt2300a")
CMT2300AComponent = cmt2300a_ns.class_("CMT2300AComponent", cg.Component, spi.SPIDevice)

CONF_CS_PIN = "cs_pin"
CONF_FCS_PIN = "fcs_pin"
CONF_GPIO1_PIN = "gpio1_pin"
CONF_GPIO2_PIN = "gpio2_pin"
CONF_GPIO3_PIN = "gpio3_pin"
CONF_FREQUENCY = "frequency"
CONF_DATA_RATE = "data_rate"

FREQUENCIES = {
    "433MHz": 433000000,
    "868MHz": 868000000,
    "915MHz": 915000000,
}

DATA_RATES = {
    "2kbps": 2000,
    "10kbps": 10000,
    "250kbps": 250000,
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CMT2300AComponent),
    cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_FCS_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_GPIO1_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_GPIO2_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_GPIO3_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_FREQUENCY, default="868MHz"): cv.enum(FREQUENCIES),
    cv.Optional(CONF_DATA_RATE, default="250kbps"): cv.enum(DATA_RATES),
}).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema(cs_pin_required=False))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    
    cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs))
    
    fcs = await cg.gpio_pin_expression(config[CONF_FCS_PIN])
    cg.add(var.set_fcs_pin(fcs))
    
    if CONF_GPIO1_PIN in config:
        gpio1 = await cg.gpio_pin_expression(config[CONF_GPIO1_PIN])
        cg.add(var.set_gpio1_pin(gpio1))
    
    if CONF_GPIO2_PIN in config:
        gpio2 = await cg.gpio_pin_expression(config[CONF_GPIO2_PIN])
        cg.add(var.set_gpio2_pin(gpio2))
    
    if CONF_GPIO3_PIN in config:
        gpio3 = await cg.gpio_pin_expression(config[CONF_GPIO3_PIN])
        cg.add(var.set_gpio3_pin(gpio3))
    
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_data_rate(config[CONF_DATA_RATE]))
