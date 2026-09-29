"""Native ESPHome component for DEYE microinverters, GEN3 and GEN4 alike, over
their local TCP/Modbus interface (Solarman V5 framing).

Covers both generations with a single component because they turn out to
share the same Modbus register map (see deyemi.h/README.md for the sources
confirming this) -- unlike TSUN, where GEN3 PLUS gets its own separate
`tsungen3` component with a genuinely different register map. "deyemi" =
"Deye MIcroinverter".

Compatible models (2 MPPT, `model: deye_2mppt`):
  GEN3: SUN600G3-EU-230, SUN800G3-EU-230, SUN1000G3-EU-230
  GEN4: SUN-M60G4-EU-Q0, SUN-M80G4-EU-Q0, SUN-M100G4-EU-Q0
Compatible models (4 MPPT, `model: deye_4mppt`):
  GEN3: SUN1300G3-EU-230, SUN1600G3-EU-230, SUN2000G3-EU-230
  GEN4: SUN-M130G4-EU-Q0, SUN-M160G4-EU-Q0, SUN-M180G4-EU-Q0,
        SUN-M200G4-EU-Q0, SUN-M220G4-EU-Q0
See README.md for full details, sources and the "Blind implementation"
caveats.

Same transport protocol as this author's `tsungen3` component: Solarman V5
envelope wrapping standard Modbus RTU, port 8899, one short-lived TCP
connection per poll.

v1 scope: read-only telemetry via a single Modbus function 0x03 request
covering registers 0x0001..0x007D, plus a `power_percent` write path
(function 0x06, register 0x0028, "Active Power Regulations"). No reset/AT+
equivalent is known to exist for this protocol, so no `button` platform.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import network
from esphome.const import CONF_ID

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["network"]
# Allows several `deyemi:` blocks (one per inverter), same as this author's
# tsungen3 component -- e.g.:
#   deyemi:
#     - id: deyemi_1
#       ip_address: 192.168.1.60
#     - id: deyemi_2
#       ip_address: 192.168.1.70
MULTI_CONF = True

deyemi_ns = cg.esphome_ns.namespace("deyemi")
DeyeMiComponent = deyemi_ns.class_("DeyeMiComponent", cg.PollingComponent)

DeyeMiModel = deyemi_ns.enum("DeyeMiModel", is_class=True)
MODELS = {
    # Auto = try to read the "Inverter ID" string register (0x0003-0x0007) at
    # startup and guess the variant from the model number it contains (e.g.
    # "SUN800G3"/"M100G4" -> 2 MPPT, "SUN2000G3"/"M160G4" -> 4 MPPT). Blind
    # implementation for the string-parsing part specifically -- falls back
    # to 2 MPPT (the more conservative option: no PV3/PV4 readings
    # published) if the string can't be parsed. Override with an explicit
    # value below if detection gets it wrong on your unit. Generation
    # (GEN3 vs GEN4) is deliberately not part of this enum -- it doesn't
    # change anything about how the register map is read.
    "auto": DeyeMiModel.MODEL_AUTO,
    "deye_2mppt": DeyeMiModel.MODEL_2MPPT,
    "deye_4mppt": DeyeMiModel.MODEL_4MPPT,
}

CONF_DEYEMI_ID = "deyemi_id"
# Same "ip_address"/"ip_port" naming as this author's tsungen3 component
# (not the generic esphome.const CONF_HOST/CONF_PORT keys).
CONF_IP_ADDRESS = "ip_address"
CONF_IP_PORT = "ip_port"
CONF_MODBUS_ADDRESS = "modbus_address"
CONF_SN = "sn"
CONF_MODEL = "model"
# Named "poll_interval" (not "update_interval") to match this author's other
# components' naming.
CONF_POLL_INTERVAL = "poll_interval"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeyeMiComponent),
        cv.Required(CONF_IP_ADDRESS): cv.string_strict,
        cv.Optional(CONF_IP_PORT, default=8899): cv.port,
        # Slave/unit id used inside the embedded Modbus RTU frame. Not confirmed on
        # real Deye hardware by this author -- 1 is the common default across
        # Solarman-based devices (incl. TSUN) but override if a scan shows otherwise.
        cv.Optional(CONF_MODBUS_ADDRESS, default=1): cv.int_range(min=1, max=247),
        # Solarman V5 "Logger Serial" (4-byte field in the frame header). Confirmed
        # by multiple independent GEN4 users (see README) to require the data
        # logger's own serial number, NOT the inverter's -- the hub logs a warning
        # at startup if left at the default of 0.
        cv.Optional(CONF_SN, default=0): cv.uint32_t,
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
    cg.add(var.set_sn(config[CONF_SN]))
    cg.add(var.set_model(config[CONF_MODEL]))
    # register_component() only auto-wires PollingComponent's interval when the
    # config key is literally "update_interval" -- ours is "poll_interval", so
    # it's set explicitly here.
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL]))
