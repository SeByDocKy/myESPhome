##############################################################################
# Composant natif ESPHome "meshcore"
#
# S'inspire directement de la structure du composant "meshtastic"
# (https://github.com/Andrik45719/esphome-meshtastic, fork qui compile ici :
#  https://github.com/SeByDocKy/myESPhome/tree/main/components/meshtastic)
# mais parle le protocole MeshCore (https://github.com/meshcore-dev/MeshCore)
# a la place du protocole Meshtastic (qui est base sur des protobuf).
#
# Contrairement a Meshtastic, MeshCore n'utilise PAS de protobuf : c'est un
# format binaire "a la main", bien plus compact. Ce composant ne reimplemente
# donc pas de code genere depuis des .proto, tout est fait a la main dans
# meshcore.h / meshcore.cpp.
#
# PERIMETRE DE CETTE PREMIERE VERSION (v1) :
#   - Messages de groupe/canal chiffres ("Group Text Message", PAYLOAD_TYPE
#     0x05), l'equivalent MeshCore du "channel" Meshtastic avec PSK partagee.
#     C'est exactement ce dont a besoin le couple master/slaves (remplace le
#     "SLAVE:<id>,..." envoye aujourd'hui via meshtastic.send_text_message).
#   - Retransmission "flood" simple (1 seul repeat, sans gestion fine de
#     l'airtime ni des delais aleatoires que fait le firmware MeshCore
#     officiel).
#
# CE QUI N'EST PAS (ENCORE) IMPLEMENTE (voir NOTES_PROTOCOLE.md) :
#   - Messages directs chiffres par ECDH X25519 (PAYLOAD_TYPE_TXT_MSG /
#     REQ / RESPONSE), qui necessitent une identite Ed25519 complete par
#     noeud (comme les "nodes" / cles publiques/privees du composant
#     meshtastic).
#   - Advertisements signes (PAYLOAD_TYPE_ADVERT) : MeshCore les authentifie
#     avec une signature Ed25519, qu'on ne genere/verifie pas ici.
#   - "Path" / "trace" / ACL room-server / multipart / control-discovery.
#   - Codes de transport (ROUTE_TYPE_TRANSPORT_FLOOD / _DIRECT).
#
# Il n'y a donc pas d'equivalent a "hw_model", "nodes:" (cles publiques),
# "exclude_pki", "position:" du composant meshtastic : ces notions n'ont
# pas d'equivalent direct cote MeshCore v1 group-only, ou necessiteraient
# l'implementation de l'identite Ed25519 (non faite ici).
##############################################################################

import base64
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sx126x, sx127x
from esphome.const import CONF_ID, CONF_NAME, CONF_TEXT, CONF_TRIGGER_ID
from esphome import automation
from esphome.automation import Trigger
from esphome.core import Lambda
from esphome.cpp_generator import ExpressionStatement, MockObj

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = []
MULTI_CONF = True

CONF_MESHCORE_ID = "meshcore_id"
CONF_LORADEVICE = "lora"
CONF_NODE_NAME = "node_name"
CONF_NODE_HASH = "node_hash"
CONF_HOP_LIMIT = "hop_limit"
CONF_REPEAT = "repeat"
CONF_CHANNELS = "channels"
CONF_CHANNEL = "channel"
CONF_PSK = "psk"
CONF_ON_PACKET = "on_packet"

meshcore_ns = cg.esphome_ns.namespace("meshcore")
MeshCore = meshcore_ns.class_("MeshCore", cg.Component)
Channel = meshcore_ns.class_("Channel")

SendGroupTextAction = meshcore_ns.class_(
    "SendGroupTextAction", automation.Action, cg.Parented.template(MeshCore)
)

# Arguments passes au lambda on_packet : (channel, from_name, text, rssi, snr)
trigger_args_with_name = [
    (cg.std_string, "channel"),
    (cg.std_string, "from_name"),
    (cg.std_string, "text"),
    (cg.float_, "rssi"),
    (cg.float_, "snr"),
]


def validate_psk(value):
    """La PSK MeshCore (GroupChannel.secret) attend 16 ou 32 octets bruts,
    encodes en base64 - exactement comme une PSK de canal Meshtastic."""
    value = cv.string_strict(value)
    try:
        decoded = base64.b64decode(value, validate=True)
    except ValueError as err:
        raise cv.Invalid("La cle doit etre encodee en base64") from err
    if len(decoded) not in (16, 32):
        raise cv.Invalid(
            "La PSK MeshCore doit faire 16 octets (AES-128, cas usuel) "
            "ou 32 octets une fois decodee du base64"
        )
    return value


CHANNEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Channel),
        cv.Required(CONF_NAME): cv.string,
        cv.Required(CONF_PSK): validate_psk,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cv.COMPONENT_SCHEMA.extend(
            {
                cv.Required(CONF_ID): cv.declare_id(MeshCore),
                cv.Optional(CONF_LORADEVICE): cv.Any(
                    cv.use_id(sx126x), cv.use_id(sx127x)
                ),
                cv.Optional(CONF_NODE_NAME, default="esphome-meshcore"): cv.string,
                # Sur un vrai reseau MeshCore, ce hash est le premier octet
                # de la cle publique Ed25519 du noeud. Comme ce composant
                # n'implemente pas encore l'identite Ed25519 (voir plus
                # haut), on le derive d'un nom ou on le laisse configurable
                # a la main pour eviter les collisions sur le "path" flood.
                cv.Optional(CONF_NODE_HASH): cv.hex_uint8_t,
                cv.Optional(CONF_HOP_LIMIT, default=3): cv.int_range(0, 63),
                cv.Optional(CONF_REPEAT, default=True): cv.boolean,
                cv.Optional(CONF_CHANNELS): cv.All(cv.ensure_list(CHANNEL_SCHEMA)),
                cv.Optional(CONF_ON_PACKET): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            Trigger.template(*[a[0] for a in trigger_args_with_name])
                        )
                    }
                ),
            }
        )
    )
)


async def to_code(configs):
    for config in configs:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)

        if CONF_LORADEVICE in config:
            loradev = await cg.get_variable(config[CONF_LORADEVICE])
            cg.add(var.set_lora(loradev))

        cg.add(var.set_node_name(config[CONF_NODE_NAME]))
        if CONF_NODE_HASH in config:
            cg.add(var.set_node_hash(config[CONF_NODE_HASH]))
        cg.add(var.set_hop_limit(config[CONF_HOP_LIMIT]))
        cg.add(var.set_repeat_enabled(config[CONF_REPEAT]))

        for channel in config.get(CONF_CHANNELS, []):
            cg.add(
                var.add_channel(
                    channel[CONF_NAME],
                    list(base64.b64decode(channel[CONF_PSK])),
                )
            )

        if on_packet := config.get(CONF_ON_PACKET):
            on_packet = on_packet[0]
            trigger = cg.new_Pvariable(on_packet[CONF_TRIGGER_ID])
            trigger = await automation.build_automation(
                trigger, trigger_args_with_name, on_packet
            )
            trigger = Lambda(
                str(
                    ExpressionStatement(
                        trigger.trigger(
                            MockObj("channel"),
                            MockObj("from_name"),
                            MockObj("text"),
                            MockObj("rssi"),
                            MockObj("snr"),
                        )
                    )
                )
            )
            trigger = await cg.process_lambda(trigger, trigger_args_with_name)
            cg.add(var.add_listener(trigger))

    # Crypto (AES-128 + SHA-256/HMAC) est implementee de facon autonome dans
    # aes_sha256.h, sans dependance a mbedtls : la premiere version de ce
    # composant utilisait mbedtls_aes_*, mais dans un projet minimal (pas de
    # "api:"/OTA-HTTPS...) rien ne force le Kconfig ESP-IDF a compiler le
    # module AES de mbedtls, ce qui provoquait un "undefined reference" au
    # link. Etre autonome evite ce genre de piege lie au projet de
    # l'utilisateur, quel que soit son sdkconfig.


SEND_GROUP_TEXT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MeshCore),
        cv.Required(CONF_CHANNEL): cv.templatable(cv.string_strict),
        cv.Required(CONF_TEXT): cv.templatable(cv.string_strict),
    }
)


@automation.register_action(
    "meshcore.send_group_text", SendGroupTextAction, SEND_GROUP_TEXT_SCHEMA
)
async def send_group_text_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_CHANNEL], args, cg.std_string)
    cg.add(var.set_channel(template_))
    template_ = await cg.templatable(config[CONF_TEXT], args, cg.std_string)
    cg.add(var.set_text(template_))
    return var
