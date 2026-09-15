import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

ez1m_ns = cg.esphome_ns.namespace("ez1m")
EZ1MComponent = ez1m_ns.class_("EZ1MComponent", cg.PollingComponent, uart.UARTDevice)

CONF_EZ1M_ID = "ez1m_id"
CONF_DC_VOLTAGE_DIVISOR = "dc_voltage_divisor"
CONF_DC_CURRENT_DIVISOR = "dc_current_divisor"
CONF_GRID_FREQUENCY_DIVISOR = "grid_frequency_divisor"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EZ1MComponent),
            cv.Optional(CONF_DC_VOLTAGE_DIVISOR, default=50.0): cv.float_,
            cv.Optional(CONF_DC_CURRENT_DIVISOR, default=88.0): cv.float_,
            cv.Optional(CONF_GRID_FREQUENCY_DIVISOR, default=27.32): cv.float_,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_dc_voltage_divisor(config[CONF_DC_VOLTAGE_DIVISOR]))
    cg.add(var.set_dc_current_divisor(config[CONF_DC_CURRENT_DIVISOR]))
    cg.add(var.set_grid_frequency_divisor(config[CONF_GRID_FREQUENCY_DIVISOR]))
