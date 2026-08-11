##############################################################################
# packet_transport: platform: meshcore
#
# Permet d'echanger des sensors/binary_sensors entre appareils ESPHome au
# travers du reseau MeshCore, en reutilisant le composant standard ESPHome
# "packet_transport" (meme mecanisme que les plateformes "udp" ou "espnow").
#
# packet_transport gere lui-meme l'encodage cle/valeur, l'association a un
# "provider" (le nom de l'appareil emetteur), l'option de code roulant, etc.
# Ce fichier ne fait que brancher packet_transport sur un canal MeshCore
# existant : l'envoi passe par MeshCore::send_group_data() (paquets
# PAYLOAD_TYPE_GRP_DATA, donnees binaires opaques - pas les messages texte
# GRP_TXT que voient les applis de chat MeshCore), et la reception s'abonne
# aux paquets GRP_DATA du canal choisi.
##############################################################################

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.cpp_types import PollingComponent

from .. import CONF_MESHCORE_ID, MeshCore, meshcore_ns

CONF_CHANNEL = "channel"

MeshCoreTransport = meshcore_ns.class_(
    "MeshCoreTransport", PacketTransport, PollingComponent
)

CONFIG_SCHEMA = transport_schema(MeshCoreTransport).extend(
    {
        cv.Required(CONF_MESHCORE_ID): cv.use_id(MeshCore),
        # Contrairement a udp/espnow, un meme "meshcore:" peut porter
        # plusieurs canaux (PSK) differents : il faut donc preciser lequel
        # utiliser pour ce transport.
        cv.Required(CONF_CHANNEL): cv.string_strict,
    }
)


async def to_code(config):
    var, providers = await new_packet_transport(config)
    meshcore_var = await cg.get_variable(config[CONF_MESHCORE_ID])
    cg.add(var.set_meshcore(meshcore_var))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
