import ipaddress

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

hmsw_ns = cg.esphome_ns.namespace("hmsw")
HMSWComponent = hmsw_ns.class_("HMSWComponent", cg.Component)

# Defined locally rather than imported from esphome.const: esphome.const's
# own CONF_HOST/CONF_PORT aren't present in every ESPHome version (broke the
# build on 2026.10.0-dev), and using them would also collide with this
# component's own field names (ip_address:/ip_port:, chosen deliberately to
# avoid ambiguity with hostnames) -- plain local strings avoid depending on
# that module's exact contents, same as CONF_NRF24L01_ID/CONF_SN/etc. in
# hm/__init__.py.
CONF_IP_ADDRESS = "ip_address"
CONF_IP_PORT = "ip_port"
CONF_POLL_INTERVAL = "poll_interval"
CONF_HEARTBEAT_INTERVAL = "heartbeat_interval"
CONF_REQUEST_TIMEOUT = "request_timeout"
CONF_DATA_SOURCE = "data_source"
CONF_ALARM_POLL_INTERVAL = "alarm_poll_interval"
CONF_STALE_DATA_REBOOT_THRESHOLD = "stale_data_reboot_threshold"
CONF_STALE_DATA_METRIC = "stale_data_metric"

STALE_DATA_METRIC_VOLTAGE = "ac_voltage"
STALE_DATA_METRIC_FREQUENCY = "ac_frequency"

DATA_SOURCE_REAL_DATA = "real_data"
DATA_SOURCE_REAL_DATA_NEW = "real_data_new"


def validate_ipv4_literal(value):
    # Deliberately NOT cv.string_strict (which would accept a hostname too):
    # this component opens a brand-new short-lived TCP socket for every
    # single request (one connection per poll/heartbeat/command, see
    # README.md), and esphome's socket::set_sockaddr() falls back to
    # lwip_getaddrinfo() -- a BLOCKING, synchronous DNS lookup -- whenever
    # its input isn't already a literal IP. Since everything else in this
    # component's request/response state machine is carefully non-blocking
    # (non-blocking sockets, deadlines checked via millis(), no delay()
    # anywhere -- see HMSWComponent::loop()/handle_connecting_()/
    # handle_sending_()/handle_receiving_()), a hostname here would be the
    # one remaining way to stall the whole ESPHome main loop, potentially
    # for seconds, on every request. Rejected at config-validation time
    # instead, so a typo'd/hostname value fails fast with a clear message
    # rather than as an intermittent field symptom.
    value = cv.string_strict(value)
    try:
        ipaddress.IPv4Address(value)
    except ValueError as err:
        raise cv.Invalid(
            f"'{value}' is not a literal IPv4 address. ip_address: only accepts a "
            "dotted-quad IP (e.g. '192.168.1.50'), not a hostname -- resolving a "
            "hostname here would require a blocking DNS lookup on every request. "
            "See README.md."
        ) from err
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HMSWComponent),
        # The IP of the HMS-XXXXW inverter itself -- it has its own
        # integrated WiFi DTU, so there is no separate radio hub to
        # reference here (unlike hm:/hms:, which point at an nrf24l01:/
        # cmt2300a: hub). Literal IPv4 only, not a hostname -- see
        # validate_ipv4_literal() above and README.md.
        cv.Required(CONF_IP_ADDRESS): validate_ipv4_literal,
        cv.Optional(CONF_IP_PORT, default=10081): cv.port,
        # 30s, not lower: per community reports on the underlying protocol
        # (suaveolent/ha-hoymiles-wifi's README), the inverter firmware
        # appears to enforce a ~30s minimum spacing between requests
        # regardless of what the client asks for, and polling below ~32s
        # (120s on newer firmware) has been reported to disable the
        # inverter's own Hoymiles-cloud sync. See README.md.
        cv.Optional(CONF_POLL_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_HEARTBEAT_INTERVAL, default="20s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_REQUEST_TIMEOUT, default="3s"): cv.positive_time_period_milliseconds,
        # "real_data" (0xA3 0x03, default) is the original, well-exercised
        # command this component was first built against. "real_data_new"
        # (0xA3 0x11) additionally exposes energy_daily, a power-limit
        # readback, and diagnostic fields, at the cost of being newer/less
        # tested here -- see README.md before switching. Either/or, not
        # both, to respect the ~2s minimum spacing the DTU firmware appears
        # to enforce between requests.
        cv.Optional(CONF_DATA_SOURCE, default=DATA_SOURCE_REAL_DATA): cv.one_of(
            DATA_SOURCE_REAL_DATA, DATA_SOURCE_REAL_DATA_NEW, lower=True
        ),
        # Alarm/warning-list feature (CMD_ACTION_ALARM_LIST), a two-step
        # request separate from poll_interval/heartbeat_interval -- see
        # README.md. Disabled (0, the default) unless set: warning data
        # changes rarely, so a long interval (e.g. 10-15 min) is plenty and
        # keeps this off the DTU firmware's ~2s minimum request spacing.
        cv.Optional(CONF_ALARM_POLL_INTERVAL, default="0s"): cv.positive_time_period_milliseconds,
        # "Hung DTU" watchdog -- ohAnd/dtuGateway's troubleshooting notes
        # describe detecting a stuck-but-still-answering DTU by watching AC
        # (grid) voltage stop changing across ~10 consecutive realtime-data
        # polls, then sending it an active reboot request to recover. 0 (the
        # default) never triggers the auto-reboot action, but the running
        # count is always tracked/published (sensor: current_stale_data) so
        # the right threshold for your setup/poll_interval can be found from
        # observed behaviour first. See README.md before enabling this.
        cv.Optional(CONF_STALE_DATA_REBOOT_THRESHOLD, default=0): cv.int_range(min=0),
        # Which quantity the watchdog above compares across polls.
        # "ac_voltage" (default) matches ohAnd/dtuGateway's own method, but
        # that field can be nearly rock-solid on an AC-coupled installation
        # behind a hybrid inverter (which tightly regulates its own AC
        # output voltage) -- prone to false "hung" positives there. Switch
        # to "ac_frequency" in that case: unlike power, it's present around
        # the clock (no false positives overnight the way power would have,
        # since power legitimately sits at 0 for hours with nothing wrong),
        # and on a grid-tied installation the hybrid inverter is normally
        # tracking the real grid frequency rather than synthesizing its
        # own, so it keeps the same natural jitter voltage might not.
        # See README.md.
        cv.Optional(CONF_STALE_DATA_METRIC, default=STALE_DATA_METRIC_VOLTAGE): cv.one_of(
            STALE_DATA_METRIC_VOLTAGE, STALE_DATA_METRIC_FREQUENCY, lower=True
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_ip_address(config[CONF_IP_ADDRESS]))
    cg.add(var.set_ip_port(config[CONF_IP_PORT]))
    cg.add(var.set_poll_interval(config[CONF_POLL_INTERVAL]))
    cg.add(var.set_heartbeat_interval(config[CONF_HEARTBEAT_INTERVAL]))
    cg.add(var.set_request_timeout(config[CONF_REQUEST_TIMEOUT]))
    cg.add(var.set_use_real_data_new(config[CONF_DATA_SOURCE] == DATA_SOURCE_REAL_DATA_NEW))
    cg.add(var.set_alarm_poll_interval(config[CONF_ALARM_POLL_INTERVAL]))
    cg.add(var.set_stale_data_threshold(config[CONF_STALE_DATA_REBOOT_THRESHOLD]))
    cg.add(var.set_stale_data_use_frequency(config[CONF_STALE_DATA_METRIC] == STALE_DATA_METRIC_FREQUENCY))
