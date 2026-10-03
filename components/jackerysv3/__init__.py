"""Native ESPHome component for the Jackery SolarVault 3 AC-coupled battery (local MQTT protocol).

The battery is an MQTT *client*: the Jackery app lets you point it at your own MQTT broker. The `mqtt_broker`
component runs a small MQTT broker on the ESP32 itself; this component (pointed to it with `mqtt_broker_id`)
decodes the Jackery JSON protocol that flows through it and exposes the result as native ESPHome entities (Home
Assistant picks them up through the native API, no MQTT integration needed):

    Jackery SolarVault 3  <--MQTT-->  mqtt_broker on the ESP32  <--native API-->  Home Assistant

Protocol reference: https://github.com/Jackery-Official/jackery (custom_components/jackery/*.py).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
import esphome.final_validate as fv

from esphome.components.mqtt_broker import CONF_MQTT_BROKER_ID, MqttBrokerComponent

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["network", "mqtt_broker"]
AUTO_LOAD = ["json"]
# One `jackerysv3:` block per battery. Several batteries can share the same `mqtt_broker` (they all connect to
# the same ESP address and port):
#   jackerysv3:
#     - id: jackery_1
#       mqtt_broker_id: broker
#       sn: "SV3XXXXXXXX1"
#     - id: jackery_2
#       mqtt_broker_id: broker
#       sn: "SV3XXXXXXXX2"
MULTI_CONF = True

jackerysv3_ns = cg.esphome_ns.namespace("jackerysv3")
JackerySV3Hub = jackerysv3_ns.class_("JackerySV3Hub", cg.PollingComponent)

CONF_JACKERYSV3_ID = "jackerysv3_id"
CONF_SN = "sn"
CONF_TOKEN = "token"
CONF_TOPIC_PREFIX = "topic_prefix"
CONF_POLL_INTERVAL = "poll_interval"
CONF_OFFLINE_TIMEOUT = "offline_timeout"
CONF_PLUG_SNS = "plug_sns"

# Maximum numbers handled by the decoder (jackery_state.h)
MAX_PV_CHANNELS = 4
MAX_PLUGS = 10


def _mqtt_topic_segment(value):
    value = cv.string_strict(value)
    if not value or any(c in value for c in "#+\0") or value.startswith("/") or value.endswith("/"):
        raise cv.Invalid("must be a non-empty MQTT topic prefix without '#', '+' or a leading / trailing '/'")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(JackerySV3Hub),
            # The embedded MQTT broker the battery connects to
            cv.GenerateID(CONF_MQTT_BROKER_ID): cv.use_id(MqttBrokerComponent),
            # Serial number of the battery, as shown in the Jackery app (used in the MQTT topics).
            cv.Required(CONF_SN): cv.string_strict,
            # Token shown in the Jackery app's MQTT settings; sent in every request. Without it (or with a wrong
            # one) the battery silently ignores the requests.
            cv.Optional(CONF_TOKEN, default=""): cv.string,
            # Topics are `<topic_prefix>/device/<sn>/status|event|action`
            cv.Optional(CONF_TOPIC_PREFIX, default="hb"): _mqtt_topic_segment,
            cv.Optional(CONF_POLL_INTERVAL, default="5s"): cv.All(
                cv.update_interval, cv.Range(min=cv.TimePeriod(seconds=1), max=cv.TimePeriod(minutes=10))
            ),
            cv.Optional(CONF_OFFLINE_TIMEOUT, default="60s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(seconds=10), max=cv.TimePeriod(hours=1)),
            ),
            # Optional: pin the smart plug slots (plug0, plug1 ...) to serial numbers. Without it the plugs are
            # assigned to the slots in the order they are discovered.
            cv.Optional(CONF_PLUG_SNS): cv.All(cv.ensure_list(cv.string_strict), cv.Length(max=MAX_PLUGS)),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def _final_validate(config):
    """Every serial number only once."""
    full = fv.full_config.get()
    for hub in full.get("jackerysv3") or []:
        if hub[CONF_ID].id != config[CONF_ID].id and hub[CONF_SN] == config[CONF_SN]:
            raise cv.Invalid(f"Serial number '{config[CONF_SN]}' is used by several jackerysv3 hubs", path=[CONF_SN])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_sn(config[CONF_SN]))
    cg.add(var.set_token(config[CONF_TOKEN]))
    cg.add(var.set_topic_prefix(config[CONF_TOPIC_PREFIX]))
    broker = await cg.get_variable(config[CONF_MQTT_BROKER_ID])
    cg.add(var.set_mqtt_broker(broker))
    cg.add(var.set_offline_timeout(config[CONF_OFFLINE_TIMEOUT]))
    for index, sn in enumerate(config.get(CONF_PLUG_SNS, [])):
        cg.add(var.set_plug_sn(index, sn))
    # register_component() only auto-wires PollingComponent's interval when the config key is literally
    # "update_interval" -- ours is "poll_interval", so it is set explicitly here.
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL]))
