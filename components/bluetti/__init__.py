"""BLUETTI Balco series (Balco 260, Balco 500) over Modbus TCP (or RTU).

Native ESPHome component: a hub (`bluetti:`) plus the sensor / text_sensor / binary_sensor platforms. It sits on top of
the ESPHome `modbus` bus and `modbus_controller` (declared in the YAML), which poll the device; the entities come from
the register table in registers.py, generated from the register library of hassio-bluetti-modbus; every platform
rejects, at validation time, a key that the selected model does not have.

The BLUETTI devices speak standard Modbus TCP (MBAP framing) on port 502 once "Modbus TCP" is enabled on their web
page. In ESPHome that is `tcp_uart` + `modbus_tcp_uart` + `modbus` (see the README).
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

DOMAIN = "bluetti"

bluetti_ns = cg.esphome_ns.namespace("bluetti")
BluettiHub = bluetti_ns.class_("BluettiHub", cg.Component)

CONF_BLUETTI_ID = "bluetti_id"
CONF_MODEL = "model"
CONF_SLOW_MODBUS_CONTROLLER_ID = "slow_modbus_controller_id"

MODELS = tuple(REGISTERS)  # balco260, balco500

ModbusController = modbus_controller.ModbusController

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BluettiHub),
        # Register map of the device: balco260 (Balco 260), balco500 (Balco 500)
        cv.Required(CONF_MODEL): cv.one_of(*MODELS, lower=True),
        # The modbus_controller (address of the device, update_interval, retries...) that polls the device
        cv.Required(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        # Optional second modbus_controller with the same address and a slower update_interval: the slow-changing
        # entities (versions, serial numbers, counters, settings...) are polled by it instead of the main one.
        cv.Optional(CONF_SLOW_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
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

    model = REGISTERS[config[CONF_MODEL]]
    cg.add(var.set_model(MODEL_NAMES[config[CONF_MODEL]]))
    # The device only answers reads that stay inside one of its register blocks and are not longer than max_span
    # registers; the hub splits the read ranges of the controllers accordingly (see BluettiHub::setup()).
    cg.add(var.set_max_span(model["max_span"]))
    for first, last in model["blocks"]:
        cg.add(var.add_block(first, last))


# ---------------------------------------------------------------------------
# Helpers shared by the platforms
# ---------------------------------------------------------------------------

# Every platform block declares `bluetti_id:` pointing at the hub declared in the top-level `bluetti:` block.
BLUETTI_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLUETTI_ID): cv.use_id(BluettiHub),
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
    raise cv.Invalid(f"BLUETTI hub '{hub_id.id}' not found")


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
        model = final_validate_hub_model(config[CONF_BLUETTI_ID])
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
