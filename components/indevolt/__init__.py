"""Native ESPHome component for INDEVOLT plug & play batteries over their local HTTP "OpenData" API.

Models: SolidFlex 2000 / PowerFlex 2000 ("Gen2") and BK1600 / BK1600 Ultra ("Gen1").

Protocol reference: the official Home Assistant integration https://github.com/INDEVOLT/homeassistant-indevolt
(indevolt_api.py). The device exposes a small JSON-RPC-like API on its own IP, port 8080:

  GET  /rpc/Sys.GetConfig                                   -> {"device": {"type", "sn", "f_ver"}, ...}
  POST /rpc/Indevolt.GetData?config={"t":[<point>, ...]}    -> {"<point>": <value>, ...}
  POST /rpc/Indevolt.SetData?config={"f":16,"t":<point>,"v":[<value>, ...]}   -> {"result": true}

Everything is addressed by numeric "points" (register-like numbers). The HTTP exchange is implemented
directly on lwIP sockets in a FreeRTOS task (no `http_request` component needed).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.const import CONF_ID, CONF_MODEL
from esphome.core import CORE

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["json"]
# One `indevolt:` block per battery (each one polls its own device):
#   indevolt:
#     - id: indevolt_1
#       ip_address: 192.168.1.70
#       model: SF2000
MULTI_CONF = True

DOMAIN = "indevolt"

indevolt_ns = cg.esphome_ns.namespace("indevolt")
IndevoltComponent = indevolt_ns.class_("IndevoltComponent", cg.PollingComponent)

CONF_INDEVOLT_ID = "indevolt_id"
CONF_IP_ADDRESS = "ip_address"
CONF_IP_PORT = "ip_port"
CONF_MAX_CHARGE_POWER = "max_charge_power"
CONF_MAX_DISCHARGE_POWER = "max_discharge_power"
CONF_MIN_POWER = "min_power"
CONF_CHARGE_TARGET_SOC = "charge_target_soc"
CONF_DISCHARGE_TARGET_SOC = "discharge_target_soc"
CONF_AUTO_REALTIME_MODE = "auto_realtime_mode"
CONF_BATCH_SIZE = "batch_size"
CONF_TIMEOUT = "timeout"
# Named "poll_interval" (not "update_interval") to match this author's zensdk / hmsw / tsungen3 components.
CONF_POLL_INTERVAL = "poll_interval"

# Number of battery packs (SFA/PFA) a SolidFlex/PowerFlex 2000 power module can drive.
MAX_PACKS = 5
# Number of PV (MPPT) inputs of a SolidFlex/PowerFlex 2000.
MAX_DC_CHANNELS = 4

# Device generations. "Gen1" = BK1600 / BK1600 Ultra, "Gen2" = SolidFlex2000 / PowerFlex2000
# (the same naming as the official Home Assistant integration's SENSORS_GEN1 / SENSORS_GEN2 tables).
GEN1 = "gen1"
GEN2 = "gen2"
MODELS = {
    "SF2000": GEN2,
    "PF2000": GEN2,
    "BK1600": GEN1,
    "BK1600_ULTRA": GEN1,
}

# Hard upper limit of the real-time control power accepted by the official integration (Gen2).
MAX_REALTIME_POWER = 10800

# (max charge W, max discharge W, minimum non-zero power W) per generation.
#   Gen2: 2400 W per unit (raise it for a cluster, up to 10800 W), 50 W minimum (HA number min).
#   Gen1: charge 0-1200 W / discharge 0-800 W (services.yaml of the HA integration).
GEN_DEFAULTS = {
    GEN2: (2400, 2400, 50),
    GEN1: (1200, 800, 0),
}


# ---------------------------------------------------------------------------
# Point tables helpers: a "points" dict maps a generation to the device point number.
# A generation missing from the dict means the entity does not exist on that generation.
# ---------------------------------------------------------------------------
def both(point):
    return {GEN1: point, GEN2: point}


def g1(point):
    return {GEN1: point}


def g2(point):
    return {GEN2: point}


def model_generation(model):
    return MODELS[model]


# ---------------------------------------------------------------------------
# Hub lookup used by the platforms
# ---------------------------------------------------------------------------
def final_hub_config(config):
    """Hub configuration of a platform entry (usable in FINAL_VALIDATE_SCHEMA)."""
    full = fv.full_config.get()
    path = full.get_path_for_id(config[CONF_INDEVOLT_ID])[:-1]
    return full.get_config_for_path(path)


def hub_generation(config):
    """Generation ("gen1"/"gen2") of the hub a platform entry is attached to (usable in to_code)."""
    hub_id = config[CONF_INDEVOLT_ID]
    hubs = CORE.config[DOMAIN]
    if isinstance(hubs, dict):
        hubs = [hubs]
    for hub in hubs:
        if hub[CONF_ID] == hub_id:
            return model_generation(hub[CONF_MODEL])
    raise cv.Invalid(f"Hub '{hub_id}' not found")  # unreachable: use_id() already validated it


def make_final_validate(entries_fn):
    """Build a FINAL_VALIDATE_SCHEMA callable.

    `entries_fn(config)` yields (label, points_dict, ...) for every entity configured on the platform;
    entities whose points_dict has no entry for the hub's generation are rejected with a clear message.
    """

    def _validate(config):
        hub = final_hub_config(config)
        model = hub[CONF_MODEL]
        gen = model_generation(model)
        for entry in entries_fn(config):
            label, points = entry[0], entry[1]
            if gen not in points:
                other = "SolidFlex/PowerFlex 2000 (model SF2000/PF2000)" if gen == GEN1 else "BK1600 / BK1600 Ultra"
                raise cv.Invalid(f"'{label}' is not available on model {model}; it only exists on {other}.")
        return config

    return _validate


def named_list(item_schema, max_items, noun, example):
    """Validator for the `dc_channels:` / `packs:` style lists: a list of single-key mappings
    ({pv0: {...}}, {pv1: {...}}, ...) whose names must be unique."""

    def _entry(value):
        value = cv.Schema({cv.string: item_schema})(value)
        if len(value) != 1:
            raise cv.Invalid(f"Each entry must contain exactly one {noun} name (e.g. '{example}').")
        return value

    def _validate(value):
        value = cv.ensure_list(_entry)(value)
        cv.Length(min=1, max=max_items)(value)
        seen = set()
        for entry in value:
            label = next(iter(entry))
            if label in seen:
                raise cv.Invalid(f"{noun.capitalize()} name '{label}' is used more than once -- each {noun} needs a unique name.")
            seen.add(label)
        return value

    return _validate


# ---------------------------------------------------------------------------
# Hub schema
# ---------------------------------------------------------------------------
def _apply_model_defaults(config):
    config = dict(config)
    charge, discharge, min_power = GEN_DEFAULTS[model_generation(config[CONF_MODEL])]
    config.setdefault(CONF_MAX_CHARGE_POWER, charge)
    config.setdefault(CONF_MAX_DISCHARGE_POWER, discharge)
    config.setdefault(CONF_MIN_POWER, min_power)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(IndevoltComponent),
            # IP address of the battery (see the INDEVOLT app or your router). The API must be enabled in the
            # INDEVOLT app ("OpenData" HTTP mode) -- see README.
            cv.Required(CONF_IP_ADDRESS): cv.string_strict,
            cv.Optional(CONF_IP_PORT, default=8080): cv.port,
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
            cv.Optional(CONF_POLL_INTERVAL, default="30s"): cv.update_interval,
            # Total time budget of one HTTP transaction (connect + send + receive).
            cv.Optional(CONF_TIMEOUT, default="8s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=500), max=cv.TimePeriod(milliseconds=60000)),
            ),
            # Number of points requested per GetData call. The official integration uses 8.
            cv.Optional(CONF_BATCH_SIZE, default=8): cv.int_range(min=1, max=16),
            cv.Optional(CONF_MAX_CHARGE_POWER): cv.int_range(min=0, max=MAX_REALTIME_POWER),
            cv.Optional(CONF_MAX_DISCHARGE_POWER): cv.int_range(min=0, max=MAX_REALTIME_POWER),
            # Requests below this power (W) are sent as "standby" (the SolidFlex/PowerFlex minimum is 50 W).
            cv.Optional(CONF_MIN_POWER): cv.int_range(min=0, max=MAX_REALTIME_POWER),
            # Third value of the real-time command (target SOC in %): charging stops at charge_target_soc,
            # discharging stops at discharge_target_soc.
            cv.Optional(CONF_CHARGE_TARGET_SOC, default=100): cv.int_range(min=0, max=100),
            cv.Optional(CONF_DISCHARGE_TARGET_SOC, default=5): cv.int_range(min=0, max=100),
            # Switch the device to "Real-Time Control" (work mode 4) before sending a power command
            # when it is not in that mode yet (what the HA integration's service does).
            cv.Optional(CONF_AUTO_REALTIME_MODE, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _apply_model_defaults,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_host(config[CONF_IP_ADDRESS]))
    cg.add(var.set_port(config[CONF_IP_PORT]))
    cg.add(var.set_gen1(model_generation(config[CONF_MODEL]) == GEN1))
    cg.add(var.set_timeout_ms(config[CONF_TIMEOUT].total_milliseconds))
    cg.add(var.set_batch_size(config[CONF_BATCH_SIZE]))
    cg.add(var.set_max_charge_power(config[CONF_MAX_CHARGE_POWER]))
    cg.add(var.set_max_discharge_power(config[CONF_MAX_DISCHARGE_POWER]))
    cg.add(var.set_min_power(config[CONF_MIN_POWER]))
    cg.add(var.set_charge_target_soc(config[CONF_CHARGE_TARGET_SOC]))
    cg.add(var.set_discharge_target_soc(config[CONF_DISCHARGE_TARGET_SOC]))
    cg.add(var.set_auto_realtime_mode(config[CONF_AUTO_REALTIME_MODE]))
    # register_component() only auto-wires PollingComponent's interval when the config key is
    # literally "update_interval" -- ours is "poll_interval", so it is set explicitly here.
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL]))
