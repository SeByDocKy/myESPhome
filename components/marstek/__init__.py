"""Marstek Venus batteries (A, D, E v1/v2, E v3) over Modbus TCP.

Native ESPHome component: a hub (`marstek:`) plus the sensor / binary_sensor / text_sensor / number / switch /
select / button platforms. The entities available depend on the `model` chosen in the hub: the register tables
in registers.py (generated from the Home Assistant integration ViperRNMC/marstek_venus_modbus) list them, and
every platform rejects, at validation time, a key that the selected model does not have.
"""

from __future__ import annotations

import math

import esphome.codegen as cg
from esphome.components import modbustcp, modbustcp_controller
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL
from esphome.core import CORE
import esphome.final_validate as fv

from .registers import MODEL_NAMES, MODEL_NOTES, REGISTERS

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["modbustcp"]
AUTO_LOAD = ["modbustcp_controller"]
MULTI_CONF = True

DOMAIN = "marstek"

marstek_ns = cg.esphome_ns.namespace("marstek")
MarstekHub = marstek_ns.class_("MarstekHub", modbustcp_controller.ModbusTCPController)

CONF_MARSTEK_ID = "marstek_id"
CONF_MODEL = "model"
CONF_LOW_PRIORITY_INTERVAL = "low_priority_interval"
CONF_COMMAND_THROTTLE = "command_throttle"
CONF_MAX_CMD_RETRIES = "max_cmd_retries"
CONF_OFFLINE_SKIP_UPDATES = "offline_skip_updates"
CONF_AUTO_RS485_CONTROL = "auto_rs485_control"

MODELS = tuple(REGISTERS)  # a, d, e_v12, e_v3

# Platforms that carry model dependent entities
PLATFORMS = ("sensor", "binary_sensor", "text_sensor", "number", "switch", "select", "button")


def _total_ms(value):
    return value.total_milliseconds if hasattr(value, "total_milliseconds") else None


def _validate_intervals(config):
    update = _total_ms(config[CONF_UPDATE_INTERVAL])
    low = _total_ms(config[CONF_LOW_PRIORITY_INTERVAL])
    if update is None or update <= 0:
        raise cv.Invalid(
            f"'{CONF_UPDATE_INTERVAL}' must be a real polling interval (not 'never')",
            path=[CONF_UPDATE_INTERVAL],
        )
    if low < update:
        raise cv.Invalid(
            f"'{CONF_LOW_PRIORITY_INTERVAL}' ({low} ms) must not be shorter than '{CONF_UPDATE_INTERVAL}' "
            f"({update} ms)",
            path=[CONF_LOW_PRIORITY_INTERVAL],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MarstekHub),
            # Register map of the battery: a (Venus A), d (Venus D), e_v12 (Venus E v1/v2), e_v3 (Venus E v3)
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
            # Slow-changing entities (energy totals, versions, schedules...) are read every
            # `low_priority_interval`, the others every `update_interval` (same split as the HA integration).
            cv.Optional(
                CONF_LOW_PRIORITY_INTERVAL, default="60s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_COMMAND_THROTTLE, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_CMD_RETRIES, default=3): cv.positive_int,
            cv.Optional(CONF_OFFLINE_SKIP_UPDATES, default=0): cv.positive_int,
            # Enable the RS485 control mode (register 42000) automatically before writing a command register
            cv.Optional(CONF_AUTO_RS485_CONTROL, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(modbustcp.modbus_device_schema(0x01)),
    _validate_intervals,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbustcp.register_modbus_device(var, config)

    cg.add(var.set_allow_duplicate_commands(False))
    cg.add(var.set_command_throttle(config[CONF_COMMAND_THROTTLE]))
    cg.add(var.set_max_cmd_retries(config[CONF_MAX_CMD_RETRIES]))
    cg.add(var.set_offline_skip_updates(config[CONF_OFFLINE_SKIP_UPDATES]))
    cg.add(var.set_auto_rs485_control(config[CONF_AUTO_RS485_CONTROL]))
    cg.add(var.set_model(MODEL_NAMES[config[CONF_MODEL]]))


# ---------------------------------------------------------------------------
# Helpers shared by the platforms
# ---------------------------------------------------------------------------

# Every platform block declares `marstek_id:` pointing at the hub declared in the top-level `marstek:` block.
MARSTEK_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MARSTEK_ID): cv.use_id(MarstekHub),
    }
)


def union_specs(platform):
    """All the keys a platform can carry, over every model, with the schema level attributes."""
    out = {}
    for model in MODELS:
        for key, spec in REGISTERS[model][platform].items():
            if key not in out:
                out[key] = dict(spec)
            elif "precision" in spec:
                out[key]["precision"] = max(out[key].get("precision", 0), spec["precision"])
    return out


def _find_hub_config(hubs, hub_id):
    for conf in hubs or []:
        if conf[CONF_ID].id == hub_id.id:
            return conf
    raise cv.Invalid(f"Marstek hub '{hub_id.id}' not found")


def hub_config(hub_id):
    """Configuration of the hub a platform points to (code generation time)."""
    return _find_hub_config(CORE.config.get(DOMAIN), hub_id)


def skip_updates(spec, hub_conf):
    """Number of `update_interval` periods to skip between two reads of an entity of this polling class."""
    if spec.get("scan", "high") != "low":
        return 0
    update = _total_ms(hub_conf[CONF_UPDATE_INTERVAL])
    low = _total_ms(hub_conf[CONF_LOW_PRIORITY_INTERVAL])
    if not update:
        return 0
    return max(0, math.ceil(low / update) - 1)


def final_validate_hub_model(hub_id_config):
    """The `model` of the hub a platform config points to (final validate time, before CORE.config is
    populated: unlike hub_config(), this reads from fv.full_config instead)."""
    full = fv.full_config.get()
    hub = _find_hub_config(full.get(DOMAIN), hub_id_config)
    return hub[CONF_MODEL]


def final_validate_platform(platform):
    """Reject the keys the model of the hub does not have."""
    keys = union_specs(platform)

    def _validate(config):
        model = final_validate_hub_model(config[CONF_MARSTEK_ID])
        available = REGISTERS[model][platform]
        for key in config:
            if key in keys and key not in available:
                others = [MODEL_NAMES[m] for m in MODELS if key in REGISTERS[m][platform]]
                raise cv.Invalid(
                    f"'{key}' is not available on the Marstek {MODEL_NAMES[model]} (model: {model}). "
                    f"It exists on: {', '.join(others)}.",
                    path=[key],
                )
        return config

    return _validate


def entity_kwargs(spec, *names):
    """Keyword arguments of the *_schema() helpers taken from a register spec (absent values are left out)."""
    mapping = {
        "unit_of_measurement": spec.get("unit"),
        "icon": spec.get("icon"),
        "entity_category": spec.get("category"),
        "device_class": spec.get("device_class"),
        "state_class": spec.get("state_class"),
        "accuracy_decimals": spec.get("precision", 0),
    }
    return {n: mapping[n] for n in names if mapping[n] is not None}
