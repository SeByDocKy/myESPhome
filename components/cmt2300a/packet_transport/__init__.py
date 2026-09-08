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
    # Un cmt2300a: piloté par un hms: (external_mode) ne peut pas aussi servir de
    # medium packet_transport générique -- même vérification/message que côté
    # runtime (CMT2300ATransport::setup()), mais détectée dès la compilation.
    try:
        full_conf = fv.full_config.get()
        hms_confs = full_conf.get("hms", [])
        if isinstance(hms_confs, dict):
            hms_confs = [hms_confs]
    except Exception:  # noqa: BLE001 -- API interne potentiellement différente
        # selon la version d'ESPHome ; on ne bloque pas la compilation pour autant.
        return config

    radio_id = config[CONF_CMT2300A_ID]
    for hms_conf in hms_confs:
        if hms_conf.get("cmt2300a_id") == radio_id:
            raise cv.Invalid(
                f"Ce cmt2300a_id ('{radio_id}') est déjà utilisé par un bloc hms: "
                f"(mode externe, bancs de registres Hoymiles) -- packet_transport ne "
                f"peut pas partager la même puce. Utilise un cmt2300a: dédié."
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var, _providers = await packet_transport.new_packet_transport(config)
    await cg.register_parented(var, config[CONF_CMT2300A_ID])
