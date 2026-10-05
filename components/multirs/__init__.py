import esphome.codegen as cg
from esphome.components import vecan
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["vecan"]
MULTI_CONF = True

CONF_MULTIRS_ID = "multirs_id"
CONF_VECAN_ID = "vecan_id"
CONF_ADDRESS = "address"
CONF_POLL_INTERVAL = "poll_interval"
CONF_BATTERY_INSTANCE = "battery_instance"
CONF_WRITE_ENABLED = "write_enabled"
CONF_LOG_UNKNOWN_REGISTERS = "log_unknown_registers"
CONF_SCAN_PAGES = "scan_pages"

multirs_ns = cg.esphome_ns.namespace("multirs")
MultiRS = multirs_ns.class_("MultiRS", cg.PollingComponent, vecan.VeCanDevice)


def _scan_page(value):
    value = cv.hex_uint16_t(value)
    if value & 0xFF:
        raise cv.Invalid(f"a register page starts at a multiple of 0x100 (got 0x{value:04X})")
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MultiRS),
        cv.Required(CONF_VECAN_ID): cv.use_id(vecan.VeCanHub),
        # Source address of the Multi RS on the VE.Can bus. The vecan hub logs the Victron devices it discovers.
        cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=253),
        cv.Optional(CONF_POLL_INTERVAL, default="10s"): cv.positive_time_period_milliseconds,
        # NMEA 2000 "battery instance" carrying the battery values in PGN 127508
        cv.Optional(CONF_BATTERY_INSTANCE, default=0): cv.int_range(min=0, max=252),
        # Nothing is ever written to the device unless this is true (switches / select then only display).
        cv.Optional(CONF_WRITE_ENABLED, default=False): cv.boolean,
        # Discovery helpers: log registers the component does not know, and whole register pages (e.g. [0x2200, 0xD000])
        cv.Optional(CONF_LOG_UNKNOWN_REGISTERS, default=False): cv.boolean,
        cv.Optional(CONF_SCAN_PAGES, default=[]): cv.ensure_list(_scan_page),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_VECAN_ID])
    cg.add(var.set_vecan(hub))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL]))
    cg.add(var.set_battery_instance(config[CONF_BATTERY_INSTANCE]))
    cg.add(var.set_write_enabled(config[CONF_WRITE_ENABLED]))
    cg.add(var.set_log_unknown_registers(config[CONF_LOG_UNKNOWN_REGISTERS]))
    for page in config[CONF_SCAN_PAGES]:
        cg.add(var.add_scan_page(page))
