"""Native ESPHome component for DEYE (also sold as VEVOR/Sunsynk/Bosswerk-rebadged)
GEN3 microinverters (e.g. SUN600G3/SUN800G3/SUN1000G3 -- 2 MPPT; SUN1300G3/SUN1600G3/
SUN2000G3 -- 4 MPPT) over their local TCP/Modbus interface (Solarman V5 framing).

This is the same transport protocol as this author's `tsungen3` component (TSUN/TSOL
GEN3 PLUS): Solarman V5 envelope wrapping standard Modbus RTU, port 8899, one
short-lived TCP connection per poll. Register maps are NOT shared between brands --
Deye's live-data block, scaling and 32-bit word order are completely different from
TSUN's. This component's register map, scaling factors and word order come from
https://github.com/StephanJoubert/home_assistant_solarman (Deye lookup files
deye_2mppt.yaml/deye_4mppt.yaml), NOT from any hands-on testing -- see README.md's
"Blind implementation" section before trusting a value at face value.

v1 scope: read-only telemetry via a single Modbus function 0x03 request covering
registers 0x0001..0x007D, plus a `power_percent` write path (function 0x06,
register 0x0028, "Active Power Regulations"). No reset/AT+ equivalent is known to
exist for this protocol, so no `button` platform (unlike tsungen3's AT+Z, which
itself turned out to be non-functional over this same client_mode transport anyway).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import network
from esphome.const import CONF_ID

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["network"]
# Allows several `deyegen3:` blocks (one per inverter), same as this author's
# tsungen3/hmsw components -- e.g.:
#   deyegen3:
#     - id: deyegen3_1
#       ip_address: 192.168.1.60
#     - id: deyegen3_2
#       ip_address: 192.168.1.61
MULTI_CONF = True

deyegen3_ns = cg.esphome_ns.namespace("deyegen3")
DeyeGen3Component = deyegen3_ns.class_("DeyeGen3Component", cg.PollingComponent)

DeyeGen3Model = deyegen3_ns.enum("DeyeGen3Model", is_class=True)
MODELS = {
    # Auto = try to read the "Inverter ID" string register (0x0003-0x0007) at
    # startup and guess the variant from the model number it contains (e.g.
    # "SUN800G3" -> 2 MPPT, "SUN2000G3" -> 4 MPPT). Blind implementation --
    # not confirmed against real hardware; falls back to 2 MPPT (the more
    # conservative option: no PV3/PV4 readings published) if the string
    # can't be parsed. Override with an explicit value below if detection
    # gets it wrong on your unit.
    "auto": DeyeGen3Model.MODEL_AUTO,
    "deye_2mppt": DeyeGen3Model.MODEL_2MPPT,
    "deye_4mppt": DeyeGen3Model.MODEL_4MPPT,
}

CONF_DEYEGEN3_ID = "deyegen3_id"
# Same "ip_address"/"ip_port" naming as this author's tsungen3 component (not the
# generic esphome.const CONF_HOST/CONF_PORT keys).
CONF_IP_ADDRESS = "ip_address"
CONF_IP_PORT = "ip_port"
CONF_MODBUS_ADDRESS = "modbus_address"
CONF_LOGGER_SERIAL = "logger_serial"
CONF_MODEL = "model"
# Named "poll_interval" (not "update_interval") to match this author's
# tsungen3/hmsw components' naming.
CONF_POLL_INTERVAL = "poll_interval"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeyeGen3Component),
        cv.Required(CONF_IP_ADDRESS): cv.string_strict,
        cv.Optional(CONF_IP_PORT, default=8899): cv.port,
        # Slave/unit id used inside the embedded Modbus RTU frame. Not confirmed on
        # real Deye GEN3 hardware -- 1 is the common default across Solarman-based
        # devices (incl. this author's own tsungen3 findings) but override if a scan
        # shows otherwise.
        cv.Optional(CONF_MODBUS_ADDRESS, default=1): cv.int_range(min=1, max=247),
        # Solarman V5 "Logger Serial" (4-byte field in the frame header). On this
        # author's TSUN GEN3 PLUS hardware, the default of 0 got NO response in
        # client_mode -- the real "Monitoring SN" had to be set instead. Assumed
        # (not yet confirmed) to behave the same way here, since it's the same V5
        # transport regardless of inverter brand. Left Optional; the hub logs a
        # warning at startup if left at 0.
        cv.Optional(CONF_LOGGER_SERIAL, default=0): cv.uint32_t,
        cv.Optional(CONF_MODEL, default="auto"): cv.enum(MODELS, lower=True),
        cv.Optional(CONF_POLL_INTERVAL, default="30s"): cv.update_interval,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_host(config[CONF_IP_ADDRESS]))
    cg.add(var.set_port(config[CONF_IP_PORT]))
    cg.add(var.set_modbus_address(config[CONF_MODBUS_ADDRESS]))
    cg.add(var.set_logger_serial(config[CONF_LOGGER_SERIAL]))
    cg.add(var.set_model(config[CONF_MODEL]))
    # register_component() only auto-wires PollingComponent's interval when the
    # config key is literally "update_interval" -- ours is "poll_interval", so
    # it's set explicitly here.
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL]))
