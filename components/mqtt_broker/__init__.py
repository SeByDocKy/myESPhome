"""Native ESPHome component: an MQTT broker running on the ESP32 itself.

Devices that talk MQTT (e.g. the Jackery SolarVault 3, see the `jackerysv3` component) connect to this broker
over the LAN; other components point to it with `mqtt_broker_id:` to decode / publish the messages and expose
the result as native ESPHome entities (no MQTT integration needed in Home Assistant).

Supported: MQTT 3.1 / 3.1.1 / 5.0 subset (CONNECT with user / password, will message, keep-alive, PUBLISH
QoS 0/1/2 acknowledged and forwarded at QoS 0, SUBSCRIBE / UNSUBSCRIBE with '+' and '#', PINGREQ, DISCONNECT).
Not supported: TLS, retained messages, persistent sessions, QoS > 0 delivery.
"""

import esphome.codegen as cg
from esphome.components import socket
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_USERNAME

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket"]
# One broker per TCP port:
#   mqtt_broker:
#     - id: broker_1
#       port: 1883
#     - id: broker_2
#       port: 1884
MULTI_CONF = True

mqtt_broker_ns = cg.esphome_ns.namespace("mqtt_broker")
MqttBrokerComponent = mqtt_broker_ns.class_("MqttBrokerComponent", cg.Component)

CONF_MQTT_BROKER_ID = "mqtt_broker_id"
CONF_MAX_CLIENTS = "max_clients"
CONF_MAX_PACKET_SIZE = "max_packet_size"
CONF_LOG_TRAFFIC = "log_traffic"


def _credentials(config):
    if (CONF_USERNAME in config) != (CONF_PASSWORD in config):
        raise cv.Invalid(f"'{CONF_USERNAME}' and '{CONF_PASSWORD}' must be given together")
    return config


def _sockets(config):
    # Listening socket + one per client (+ 1 spare to accept and refuse an extra connection)
    socket.consume_sockets(1, "mqtt_broker", socket.SocketType.TCP_LISTEN)(config)
    socket.consume_sockets(config[CONF_MAX_CLIENTS] + 1, "mqtt_broker")(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MqttBrokerComponent),
            cv.Optional(CONF_PORT, default=1883): cv.port,
            # Without credentials any client is accepted.
            cv.Optional(CONF_USERNAME): cv.string_strict,
            cv.Optional(CONF_PASSWORD): cv.string_strict,
            cv.Optional(CONF_MAX_CLIENTS, default=4): cv.int_range(min=1, max=8),
            # Larger PUBLISH packets are discarded (other oversized packets drop the client).
            cv.Optional(CONF_MAX_PACKET_SIZE, default=8192): cv.int_range(min=512, max=65535),
            # Log every MQTT packet (CONNECT / SUBSCRIBE / PUBLISH with a payload preview) at INFO level.
            cv.Optional(CONF_LOG_TRAFFIC, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _credentials,
    _sockets,
)


def _final_validate(config):
    import esphome.final_validate as fv

    ports = {}
    for hub in fv.full_config.get().get("mqtt_broker") or []:
        port = hub[CONF_PORT]
        if port in ports and ports[port] != hub[CONF_ID].id and hub[CONF_ID].id == config[CONF_ID].id:
            raise cv.Invalid(f"Port {port} is already used by mqtt_broker '{ports[port]}'", path=[CONF_PORT])
        ports.setdefault(port, hub[CONF_ID].id)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    if CONF_USERNAME in config:
        cg.add(var.set_username(config[CONF_USERNAME]))
        cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_max_clients(config[CONF_MAX_CLIENTS]))
    cg.add(var.set_max_packet_size(config[CONF_MAX_PACKET_SIZE]))
    cg.add(var.set_log_traffic(config[CONF_LOG_TRAFFIC]))
