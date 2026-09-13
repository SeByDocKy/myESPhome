import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import packet_transport

from .. import CMT2300AComponent, cmt2300a_ns

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["cmt2300a"]

CONF_CMT2300A_ID = "cmt2300a_id"

CMT2300ATransport = cmt2300a_ns.class_(
    "CMT2300ATransport",
    packet_transport.PacketTransport,
    cg.Parented.template(CMT2300AComponent),
)

CONFIG_SCHEMA = packet_transport.transport_schema(CMT2300ATransport).extend(
    {
        cv.GenerateID(CONF_CMT2300A_ID): cv.use_id(CMT2300AComponent),
    }
)


def _final_validate(config):
    # A cmt2300a: driven by an hms: (external_mode) cannot also serve as a
    # generic packet_transport medium -- same check/message as the runtime one
    # (CMT2300ATransport::setup()), but caught at compile time here.
    try:
        full_conf = fv.full_config.get()
        hms_confs = full_conf.get("hms", [])
        if isinstance(hms_confs, dict):
            hms_confs = [hms_confs]
    except Exception:  # noqa: BLE001 -- internal API may differ across
        # ESPHome versions; don't block compilation because of that.
        return config

    radio_id = config[CONF_CMT2300A_ID]
    for hms_conf in hms_confs:
        if hms_conf.get("cmt2300a_id") == radio_id:
            raise cv.Invalid(
                f"This cmt2300a_id ('{radio_id}') is already used by an hms: block "
                f"(external mode, Hoymiles register banks) -- packet_transport cannot "
                f"share the same chip. Use a dedicated cmt2300a: instead."
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var, _providers = await packet_transport.new_packet_transport(config)
    await cg.register_parented(var, config[CONF_CMT2300A_ID])
