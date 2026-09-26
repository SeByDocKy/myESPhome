"""Native ESPHome component for TSUN/TSOL GEN3 PLUS micro-inverters (e.g. MX1000, MX3000,
MX450, MS1600/1800/2000) over their local TCP/Modbus interface (Solarman V5 framing).

Protocol reference: https://github.com/s-allius/tsun-gen3-proxy (GEN3 PLUS "client_mode")
Frame reference:    https://pysolarmanv5.readthedocs.io/en/stable/solarmanv5_protocol.html

v1 scope is read-only telemetry via Modbus function 0x03 (Read Holding Registers),
polling the 0x3000..0x302A live-data block in a single request per cycle. No AT+ commands,
no MODBUS writes (rated power / output coefficient) yet -- same incremental approach as
this author's other Hoymiles/TSUN-adjacent components (hm, hms, hmsw, pcm3k6w).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import network
from esphome.const import CONF_ID, CONF_PORT

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["network"]
# Allows several `tsungen3:` blocks (one per inverter), same as this author's
# hmsw component -- e.g.:
#   tsungen3:
#     - id: tsungen3_1
#       host: 192.168.1.50
#     - id: tsungen3_2
#       host: 192.168.1.51
MULTI_CONF = True

tsungen3_ns = cg.esphome_ns.namespace("tsungen3")
TSunGen3Component = tsungen3_ns.class_("TSunGen3Component", cg.PollingComponent)

CONF_TSUNGEN3_ID = "tsungen3_id"
CONF_HOST = "host"
CONF_MODBUS_ADDRESS = "modbus_address"
CONF_LOGGER_SERIAL = "logger_serial"
# Named "poll_interval" (not "update_interval") to match this author's hmsw
# component's naming.
CONF_POLL_INTERVAL = "poll_interval"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TSunGen3Component),
        cv.Required(CONF_HOST): cv.string_strict,
        cv.Optional(CONF_PORT, default=8899): cv.port,
        # Slave/unit id used inside the embedded Modbus RTU frame. TSUN GEN3 PLUS
        # inverters have been observed responding to 0x01; not confirmed across all
        # firmware/models -- override if a scan shows otherwise.
        cv.Optional(CONF_MODBUS_ADDRESS, default=1): cv.int_range(min=1, max=247),
        # Solarman V5 "Logger Serial" (4-byte field in the frame header). Confirmed
        # against a real MX1000 (Sep 2026): the default of 0 gets NO response in
        # client_mode -- this must be set to the real "Monitoring SN" printed on
        # the inverter's sticker. Left Optional (rather than Required) since this
        # was only confirmed on one unit/firmware; the hub also logs a warning at
        # startup if left at 0.
        cv.Optional(CONF_LOGGER_SERIAL, default=0): cv.uint32_t,
        cv.Optional(CONF_POLL_INTERVAL, default="30s"): cv.update_interval,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_modbus_address(config[CONF_MODBUS_ADDRESS]))
    cg.add(var.set_logger_serial(config[CONF_LOGGER_SERIAL]))
    # register_component() only auto-wires PollingComponent's interval when the
    # config key is literally "update_interval" -- ours is "poll_interval", so
    # it's set explicitly here.
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL]))
