"""Anker SOLIX Solarbank batteries (Max AC, Max, XE, XE AC, 4 E5000 Pro) over Modbus (RTU / RS485, or TCP through a
serial-over-TCP bridge).

Native ESPHome component: a hub (`anker_solix:`) plus the sensor / binary_sensor / text_sensor / number / select /
output platforms. It sits on top of the ESPHome `modbus` bus and `modbus_controller` (declared in the YAML), which
poll the battery; the entities come from the register table in registers.py, generated from the device configuration
files of the official Anker Home Assistant integration (ha-anker-solix-official); every platform rejects, at
validation time, a key that the selected model does not have.
"""

from __future__ import annotations

import esphome.codegen as cg
from esphome.components import modbus_controller
from esphome.components.modbus_controller import CONF_MODBUS_CONTROLLER_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
import esphome.final_validate as fv

from .registers import MODEL_NAMES, REGISTERS

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["modbus_controller"]
DEPENDENCIES = ["modbus_controller"]
MULTI_CONF = True

DOMAIN = "anker_solix"

anker_solix_ns = cg.esphome_ns.namespace("anker_solix")
AnkerSolixHub = anker_solix_ns.class_("AnkerSolixHub", cg.Component)

CONF_ANKER_SOLIX_ID = "anker_solix_id"
CONF_MODEL = "model"
CONF_SLOW_MODBUS_CONTROLLER_ID = "slow_modbus_controller_id"
CONF_AUTO_THIRD_PARTY_CONTROL = "auto_third_party_control"
CONF_MAX_CHARGE_POWER = "max_charge_power"
CONF_MAX_DISCHARGE_POWER = "max_discharge_power"
CONF_SETPOINT_REFRESH = "setpoint_refresh"
CONF_READ_CAPABILITY_MASK = "read_capability_mask"

MODELS = tuple(REGISTERS)  # max_ac, max, sb4_e5000_pro, xe

ModbusController = modbus_controller.ModbusController

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AnkerSolixHub),
        # Register map of the battery: max_ac (Solarbank Max AC, XE AC), max (Solarbank Max, XE),
        # xe (XE), sb4_e5000_pro (Solarbank 4 E5000 Pro)
        cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
        # The modbus_controller (address of the battery, update_interval, retries...) that polls the battery
        cv.Required(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        # Optional second modbus_controller with the same address and a slower update_interval: the slow-changing
        # entities (energy totals, versions, SOC limits...) are polled by it instead of the main one.
        cv.Optional(CONF_SLOW_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        # Select the "Third-Party Controlled" operating mode (the only one in which the power setpoint is
        # applied) before a setpoint is written, and again if the Anker app changed it.
        cv.Optional(CONF_AUTO_THIRD_PARTY_CONTROL, default=False): cv.boolean,
        # Optional limits (W) of the power setpoint, on top of what the battery reports it can do
        cv.Optional(CONF_MAX_CHARGE_POWER): cv.int_range(min=1, max=100000),
        cv.Optional(CONF_MAX_DISCHARGE_POWER): cv.int_range(min=1, max=100000),
        # Re-send the last power setpoint when nothing was written for this long (0 = never)
        cv.Optional(
            CONF_SETPOINT_REFRESH, default="0ms"
        ): cv.positive_time_period_milliseconds,
        # Read register 0x8007 (SOC limit capabilities). A firmware without it answers "Illegal data address" at
        # every poll: set false then, the SOC limit functions are assumed to be supported.
        cv.Optional(CONF_READ_CAPABILITY_MASK, default=True): cv.boolean,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    controller = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_controller(controller))
    if CONF_SLOW_MODBUS_CONTROLLER_ID in config:
        slow = await cg.get_variable(config[CONF_SLOW_MODBUS_CONTROLLER_ID])
        cg.add(var.set_slow_controller(slow))

    cg.add(var.set_model(MODEL_NAMES[config[CONF_MODEL]]))
    cg.add(var.set_auto_third_party_control(config[CONF_AUTO_THIRD_PARTY_CONTROL]))
    if CONF_MAX_CHARGE_POWER in config:
        cg.add(var.set_max_charge_power(config[CONF_MAX_CHARGE_POWER]))
    if CONF_MAX_DISCHARGE_POWER in config:
        cg.add(var.set_max_discharge_power(config[CONF_MAX_DISCHARGE_POWER]))
    cg.add(var.set_setpoint_refresh(config[CONF_SETPOINT_REFRESH]))
    cg.add(var.set_read_capability_mask(config[CONF_READ_CAPABILITY_MASK]))


# ---------------------------------------------------------------------------
# Helpers shared by the platforms
# ---------------------------------------------------------------------------

# Every platform block declares `anker_solix_id:` pointing at the hub declared in the top-level `anker_solix:` block.
ANKER_SOLIX_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ANKER_SOLIX_ID): cv.use_id(AnkerSolixHub),
    }
)


def is_slow(spec):
    """True for an entity of the slow polling class (polled by the slow controller when there is one)."""
    return spec.get("scan", "high") == "low"


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
    raise cv.Invalid(f"Anker SOLIX hub '{hub_id.id}' not found")


def hub_config(hub_id):
    """Configuration of the hub a platform points to (code generation time)."""
    return _find_hub_config(CORE.config.get(DOMAIN), hub_id)


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
        model = final_validate_hub_model(config[CONF_ANKER_SOLIX_ID])
        available = REGISTERS[model][platform]
        for key in config:
            if key in keys and key not in available:
                others = [MODEL_NAMES[m] for m in MODELS if key in REGISTERS[m][platform]]
                raise cv.Invalid(
                    f"'{key}' is not available on the {MODEL_NAMES[model]} (model: {model}). "
                    f"It exists on: {', '.join(others) or 'no model'}.",
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
