import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, esp32_ble_tracker
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    CONF_SERVICE_UUID,
    CONF_CHARACTERISTIC_UUID,
)

DEPENDENCIES = ["ble_client"]
CODEOWNERS = ["@SeByDocKy", "@claude"]
MULTI_CONF = True

# Certains modules (dont le RYLR999 testé en pratique) exposent DEUX
# caractéristiques séparées dans le même service - une en écriture
# (READ | WRITE_NO_RESPONSE) et une autre en notification seule (NOTIFY) -
# alors que la doc REYAX documente une unique caractéristique combinant les
# deux. Ce champ est optionnel : s'il est omis, on réutilise
# characteristic_uuid pour la notification (cas de la doc officielle).
CONF_NOTIFY_CHARACTERISTIC_UUID = "notify_characteristic_uuid"
CONF_RYLR999_BLE_ID = "rylr999_ble_id"
CONF_ADDRESS = "address"
CONF_SPREADING_FACTOR = "spreading_factor"
CONF_SIGNAL_BANDWIDTH = "signal_bandwidth"
CONF_CODING_RATE = "coding_rate"
CONF_PREAMBLE_LENGTH = "preamble_length"
CONF_NETWORK_ID = "network_id"
CONF_TX_POWER = "tx_power"
CONF_AIR_TIME = "air_time"
CONF_MIN_TX_INTERVAL = "min_tx_interval"

rylr999_ble_ns = cg.esphome_ns.namespace("rylr999_ble")

RYLR999BLEComponent = rylr999_ble_ns.class_(
    "RYLR999BLEComponent", cg.Component, ble_client.BLEClientNode
)
# Interface observateur utilisée par la plateforme packet_transport pour
# recevoir les paquets décodés depuis les trames "+RCV=..." (cf. rylr999_ble.h).
RYLR999BLEListener = rylr999_ble_ns.class_("RYLR999BLEListener")


def validate_bandwidth(value):
    value = cv.frequency(value)
    if value not in (125000, 250000, 500000):
        raise cv.Invalid("Bandwidth must be 125kHz, 250kHz or 500kHz")
    return value


def validate_spreading_factor(value):
    value = cv.int_(value)
    if value < 5 or value > 11:
        raise cv.Invalid("Spreading factor must be between 5 and 11")
    return value


def validate_coding_rate(value):
    value = cv.int_(value)
    if value < 1 or value > 4:
        raise cv.Invalid("Coding rate must be between 1 and 4")
    return value


def validate_network_id(value):
    value = cv.int_(value)
    valid_ids = list(range(3, 16)) + [18]
    if value not in valid_ids:
        raise cv.Invalid("Network ID must be 3-15 or 18")
    return value


def _validate_preamble_networkid(config):
    # Doc RYLR999 : preamble_length n'est réglable (4-24) que si network_id: 18,
    # sinon il doit rester à 12 (même contrainte que pour la variante rylr999 du
    # composant UART rylr998 - cf. AT+PARAMETER dans le guide REYAX).
    if config[CONF_NETWORK_ID] != 18 and config[CONF_PREAMBLE_LENGTH] != 12:
        raise cv.Invalid(
            "preamble_length n'est réglable (4-24) que si network_id: 18. "
            "Pour toute autre valeur de network_id, preamble_length doit "
            "rester à 12 (valeur par défaut)."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RYLR999BLEComponent),
            cv.Required(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
            cv.Required(CONF_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_NOTIFY_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=65535),
            cv.Optional(CONF_FREQUENCY, default=915000000): cv.frequency,
            cv.Optional(CONF_SPREADING_FACTOR, default=9): validate_spreading_factor,
            cv.Optional(CONF_SIGNAL_BANDWIDTH, default=125000): validate_bandwidth,
            cv.Optional(CONF_CODING_RATE, default=1): validate_coding_rate,
            cv.Optional(CONF_PREAMBLE_LENGTH, default=12): cv.int_range(min=4, max=24),
            cv.Optional(CONF_NETWORK_ID, default=18): validate_network_id,
            # RYLR999 : PA jusqu'à 30dBm (contre 22dBm max sur RYLR998/SX1262).
            cv.Optional(CONF_TX_POWER, default=22): cv.int_range(min=0, max=30),
            cv.Optional(CONF_AIR_TIME, default=False): cv.boolean,
            # Espacement mini forcé entre deux AT+SEND, quelle qu'en soit la
            # cause (packet_transport déclenche un envoi à la fois sur son
            # update_interval ET immédiatement à chaque changement de valeur
            # d'un sensor suivi - cf. loop() de packet_transport - donc deux
            # envois peuvent partir à quelques centaines de ms d'écart).
            # Le RYLR999 rejette (+ERR=17 "Last TX was not completed") tout
            # AT+SEND lancé avant la fin d'antenne du précédent ; le défaut
            # 1s est une marge prudente pour SF9/125kHz sur ~80 octets -
            # réduisez si votre SF/BW/payload est plus rapide, augmentez si
            # vous voyez encore des +ERR=17.
            cv.Optional(CONF_MIN_TX_INTERVAL, default="1s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA),
    _validate_preamble_networkid,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    service_uuid = config[CONF_SERVICE_UUID]
    if len(service_uuid) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(var.set_service_uuid16(esp32_ble_tracker.as_hex(service_uuid)))
    elif len(service_uuid) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(var.set_service_uuid32(esp32_ble_tracker.as_hex(service_uuid)))
    elif len(service_uuid) == len(esp32_ble_tracker.bt_uuid128_format):
        cg.add(
            var.set_service_uuid128(
                esp32_ble_tracker.as_reversed_hex_array(service_uuid)
            )
        )

    char_uuid = config[CONF_CHARACTERISTIC_UUID]
    if len(char_uuid) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(var.set_char_uuid16(esp32_ble_tracker.as_hex(char_uuid)))
    elif len(char_uuid) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(var.set_char_uuid32(esp32_ble_tracker.as_hex(char_uuid)))
    elif len(char_uuid) == len(esp32_ble_tracker.bt_uuid128_format):
        cg.add(
            var.set_char_uuid128(esp32_ble_tracker.as_reversed_hex_array(char_uuid))
        )

    # Caractéristique de notification : celle dédiée si fournie, sinon la
    # même que l'écriture (cas d'une caractéristique combinée NOTIFY+WRITE).
    notify_uuid = config.get(CONF_NOTIFY_CHARACTERISTIC_UUID, char_uuid)
    if len(notify_uuid) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(var.set_notify_char_uuid16(esp32_ble_tracker.as_hex(notify_uuid)))
    elif len(notify_uuid) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(var.set_notify_char_uuid32(esp32_ble_tracker.as_hex(notify_uuid)))
    elif len(notify_uuid) == len(esp32_ble_tracker.bt_uuid128_format):
        cg.add(
            var.set_notify_char_uuid128(
                esp32_ble_tracker.as_reversed_hex_array(notify_uuid)
            )
        )

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_spreading_factor(config[CONF_SPREADING_FACTOR]))
    cg.add(var.set_bandwidth(config[CONF_SIGNAL_BANDWIDTH]))
    cg.add(var.set_coding_rate(config[CONF_CODING_RATE]))
    cg.add(var.set_preamble_length(config[CONF_PREAMBLE_LENGTH]))
    cg.add(var.set_network_id(config[CONF_NETWORK_ID]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))
    cg.add(var.set_air_time(config[CONF_AIR_TIME]))
    cg.add(var.set_min_tx_interval_ms(config[CONF_MIN_TX_INTERVAL].total_milliseconds))
