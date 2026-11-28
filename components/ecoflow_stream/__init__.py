"""EcoFlow STREAM 5000 / STREAM AC 5000 batteries over local Modbus TCP.

Native ESPHome component: a hub (`ecoflow_stream:`) plus the sensor / text_sensor / binary_sensor / switch / number /
select / output platforms. It sits on top of the ESPHome `modbus` bus and `modbus_controller` (declared in the YAML),
which poll the device; the entities come from the register table in registers.py, generated from the official EcoFlow
Home Assistant integration; every platform rejects, at validation time, a key that the selected model does not have.

The battery speaks standard Modbus TCP (MBAP framing) on port 502 once "Modbus TCP" is enabled in the EcoFlow app. In
ESPHome that is `tcp_uart` + `modbus_tcp_uart` + `modbus` (see the README).
"""

from __future__ import annotations

import esphome.codegen as cg
from esphome.components import modbus_controller
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE
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

DOMAIN = "ecoflow_stream"

ecoflow_stream_ns = cg.esphome_ns.namespace("ecoflow_stream")
EcoflowStreamHub = ecoflow_stream_ns.class_("EcoflowStreamHub", cg.Component)
ControlPoint = ecoflow_stream_ns.enum("ControlPoint", True)

CONF_ECOFLOW_STREAM_ID = "ecoflow_stream_id"
CONF_MODEL = "model"
CONF_SLOW_MODBUS_CONTROLLER_ID = "slow_modbus_controller_id"
CONF_HEARTBEAT_INTERVAL = "heartbeat_interval"
CONF_IGNORE_MASTER_FLAG = "ignore_master_flag"
CONF_CHARGE_SIGN = "charge_sign"
CONF_BATTERY_POWER_CHARGE_SIGN = "battery_power_charge_sign"
CONF_CONTROL_POINT = "control_point"
CONF_AUTO_CONTROL_POINT = "auto_control_point"
CONF_MAX_CHARGE_POWER = "max_charge_power"
CONF_MAX_DISCHARGE_POWER = "max_discharge_power"
CONF_SETPOINT_REFRESH = "setpoint_refresh"

MODELS = tuple(REGISTERS)  # stream5000, stream_ac5000

# Control points the charge / discharge outputs can drive (the meter point is a grid exchange target, not a
# charge / discharge power: use the `targets.meter_power` number for it)
CONTROL_POINTS = {
    "battery": ControlPoint.BATTERY,
    "inverter": ControlPoint.INVERTER,
    "grid_port": ControlPoint.GRID_PORT,
}
# Sign of the target register that CHARGES the battery. The official sources do not document it.
CHARGE_SIGNS = {"negative": -1, "positive": 1}
# Target powers converted with `charge_sign` (the meter target is a grid power, not a battery power)
SIGNED_TARGETS = ("targets.battery_power", "targets.inverter_power", "targets.grid_port_power")

ModbusController = modbus_controller.ModbusController


def _heartbeat_interval(value):
    value = cv.positive_time_period_milliseconds(value)
    if value.total_milliseconds and not 1000 <= value.total_milliseconds <= 50000:
        raise cv.Invalid("must be 0 (disabled) or between 1s and 50s: the battery expects one at least every 60s")
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EcoflowStreamHub),
        cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
        # The modbus_controller (address of the device, update_interval, retries...) that polls the device
        cv.Required(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        # Optional second modbus_controller with the same address and a slower update_interval
        cv.Optional(CONF_SLOW_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        # Keep-alive written to the battery while a control platform is declared (0 = never)
        cv.Optional(CONF_HEARTBEAT_INTERVAL, default="20s"): _heartbeat_interval,
        # Publish and write the system values even when the unit does not report itself as the system master
        cv.Optional(CONF_IGNORE_MASTER_FLAG, default=False): cv.boolean,
        # Needed by the output platform only
        cv.Optional(CONF_CHARGE_SIGN): cv.enum(CHARGE_SIGNS, lower=True),
        cv.Optional(CONF_BATTERY_POWER_CHARGE_SIGN, default="positive"): cv.enum(CHARGE_SIGNS, lower=True),
        cv.Optional(CONF_CONTROL_POINT, default="battery"): cv.enum(CONTROL_POINTS, lower=True),
        cv.Optional(CONF_AUTO_CONTROL_POINT, default=False): cv.boolean,
        cv.Optional(CONF_MAX_CHARGE_POWER): cv.int_range(min=1, max=32767),
        cv.Optional(CONF_MAX_DISCHARGE_POWER): cv.int_range(min=1, max=32767),
        # Re-send the last power setpoint when nothing was written for this long (0 = never)
        cv.Optional(CONF_SETPOINT_REFRESH, default="0ms"): cv.positive_time_period_milliseconds,
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
    cg.add(var.set_heartbeat_interval(config[CONF_HEARTBEAT_INTERVAL]))
    cg.add(var.set_ignore_master_flag(config[CONF_IGNORE_MASTER_FLAG]))
    if CONF_CHARGE_SIGN in config:
        cg.add(var.set_charge_sign(CHARGE_SIGNS[config[CONF_CHARGE_SIGN]]))
    cg.add(var.set_battery_power_charge_sign(CHARGE_SIGNS[config[CONF_BATTERY_POWER_CHARGE_SIGN]]))
    cg.add(var.set_control_point(config[CONF_CONTROL_POINT]))
    cg.add(var.set_auto_control_point(config[CONF_AUTO_CONTROL_POINT]))
    if CONF_MAX_CHARGE_POWER in config:
        cg.add(var.set_max_charge_power(config[CONF_MAX_CHARGE_POWER]))
    if CONF_MAX_DISCHARGE_POWER in config:
        cg.add(var.set_max_discharge_power(config[CONF_MAX_DISCHARGE_POWER]))
    cg.add(var.set_setpoint_refresh(config[CONF_SETPOINT_REFRESH]))


def configure_item(var, hub, spec):
    """Code common to every entity that reads a register."""
    cg.add(
        var.configure(
            spec["register"],
            spec["count"],
            SENSOR_VALUE_TYPE[spec["vtype"]],
            MODBUS_REGISTER_TYPE["holding"],
        )
    )
    if spec["system"]:
        cg.add(var.set_system_value(hub))
    # Signed battery powers are shown as positive = charge, whatever the device does
    path = ".".join(spec["path"])
    if path in SIGNED_TARGETS:
        cg.add(var.set_power_sign(hub.target_sign()))
    elif path == "battery.power":
        cg.add(var.set_power_sign(hub.measure_sign()))
    cg.add(hub.add_item(var, is_slow(spec)))


# ---------------------------------------------------------------------------
# Helpers shared by the platforms
# ---------------------------------------------------------------------------

# Every platform block declares `ecoflow_stream_id:` pointing at the hub declared in the top-level `ecoflow_stream:` block.
ECOFLOW_STREAM_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECOFLOW_STREAM_ID): cv.use_id(EcoflowStreamHub),
    }
)


def is_slow(spec):
    """True for an entity of the slow polling class (polled by the slow controller when there is one)."""
    return spec.get("scan", "high") == "low"


def union_specs(platform):
    """All the keys a platform can carry, over every model: key -> spec (the first model that has it)."""
    out = {}
    for model in MODELS:
        for key, spec in REGISTERS[model]["specs"][platform].items():
            if key not in out:
                out[key] = dict(spec)
            elif "precision" in spec:
                out[key]["precision"] = max(out[key].get("precision", 0), spec["precision"])
    return out


def build_tree(specs):
    """Nested structure of the YAML: a node is a dict, a leaf is the flat register key (a str)."""
    tree = {}
    for key, spec in specs.items():
        node = tree
        for part in spec["path"][:-1]:
            node = node.setdefault(part, {})
        node[spec["path"][-1]] = key
    return tree


def tree_schema(tree, leaf_schema):
    """cv.Schema of the nested structure; leaf_schema(flat key) returns the schema of an entity."""
    out = {}
    for name, child in tree.items():
        out[cv.Optional(name)] = leaf_schema(child) if isinstance(child, str) else tree_schema(child, leaf_schema)
    return cv.Schema(out)


def walk_config(tree, config, path=()):
    """Yield (flat register key, YAML path, entity config) for every entity the user configured."""
    for name, value in config.items():
        child = tree.get(name) if isinstance(tree, dict) else None
        if child is None:
            continue  # id, bluetti_id, ...
        if isinstance(child, str):
            yield child, [*path, name], value
        else:
            yield from walk_config(child, value, (*path, name))


def _find_hub_config(hubs, hub_id):
    for conf in hubs or []:
        if conf[CONF_ID].id == hub_id.id:
            return conf
    raise cv.Invalid(f"EcoFlow STREAM hub '{hub_id.id}' not found")


def hub_config(hub_id):
    """Configuration of the hub a platform points to (code generation time)."""
    return _find_hub_config(CORE.config.get(DOMAIN), hub_id)


def final_validate_hub_model(hub_id_config):
    """The `model` of the hub a platform config points to (final validate time, before CORE.config is
    populated: unlike hub_config(), this reads from fv.full_config instead)."""
    full = fv.full_config.get()
    hub = _find_hub_config(full.get(DOMAIN), hub_id_config)
    return hub[CONF_MODEL]


def final_validate_platform(platform, tree, specs):
    """Reject the keys the model of the hub does not have."""

    def _validate(config):
        model = final_validate_hub_model(config[CONF_ECOFLOW_STREAM_ID])
        available = REGISTERS[model]["specs"][platform]
        for key, path, _conf in walk_config(tree, config):
            if key in specs and key not in available:
                others = [MODEL_NAMES[m] for m in MODELS if key in REGISTERS[m]["specs"][platform]]
                raise cv.Invalid(
                    f"'{'.'.join(path)}' is not available on the {MODEL_NAMES[model]} (model: {model}). "
                    f"It exists on: {', '.join(others) or 'no model'}.",
                    path=path,
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
