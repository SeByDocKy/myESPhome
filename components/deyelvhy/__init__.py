"""Deye low-voltage hybrid inverters (single-phase and three-phase) over Modbus RTU.

Native ESPHome component: a hub (`deyelvhy:`) plus the sensor / binary_sensor / switch / number / select / text_sensor
platforms, each offered twice: `deyelvhy_fast` and `deyelvhy_slow`. It sits on top of the ESPHome `modbus` bus and
`modbus_controller` (declared in the YAML): the controllers poll the inverter and own the `update_interval`, one for
the fast entities and, optionally, a second one for the slow entities. The entities come from the register table of
the selected `phase` (registers.py). The total-energy part (`total_daily_energy`, the "yesterday" values) stays in
the YAML.

The hub adds to the plain `modbus_controller` entities:
  - a write that only changes its own bits of a register shared by several switches / selects (read-modify-write on a
    copy of the registers last read), where the stock modbus switch overwrites the whole register;
  - a hold-off after a write, so the value read back by a poll that was already under way does not undo it;
  - the calculated sensors (battery charge / discharge, essential / non-essential load...), which read their own
    registers and are computed in C++ instead of by polling template sensors.

The code of each platform is in its own sub-directory of this one (deyelvhy/<platform>/: `__init__.py` for the Python side,
deyelvhy_<platform>.* for the C++ side); `deyelvhy_fast/<platform>/` and `deyelvhy_slow/<platform>/` only declare the
platforms and say which controller they use.
"""

from __future__ import annotations

import esphome.codegen as cg
from esphome.components import modbus_controller
from esphome.components.modbus.helpers import SENSOR_VALUE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
import esphome.final_validate as fv

from .registers import PHASE_NAMES, REGISTERS

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["modbus_controller"]
DEPENDENCIES = ["modbus_controller"]
MULTI_CONF = True

DOMAIN = "deyelvhy"

deyelvhy_ns = cg.esphome_ns.namespace("deyelvhy")
DeyeLvHyHub = deyelvhy_ns.class_("DeyeLvHyHub", cg.Component)

CONF_DEYELVHY_ID = "deyelvhy_id"
CONF_PHASE = "phase"
CONF_FAST_MODBUS_CONTROLLER_ID = "fast_modbus_controller_id"
CONF_SLOW_MODBUS_CONTROLLER_ID = "slow_modbus_controller_id"
CONF_INVERTER_FACTOR = "inverter_factor"
CONF_USE_WRITE_MULTIPLE = "use_write_multiple"
CONF_WRITE_HOLD = "write_hold"

PHASES = tuple(REGISTERS)  # 1p, 3p

ModbusController = modbus_controller.ModbusController

def inverter_factor(value):
    """`auto`, or a positive number."""
    if isinstance(value, str) and value.strip().lower() == "auto":
        return "auto"
    return cv.positive_float(value)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeyeLvHyHub),
        # 1p: single-phase, 3p: three-phase. Selects the register table.
        cv.Required(CONF_PHASE): cv.one_of(*PHASES, lower=True),
        # The modbus_controller (address of the inverter, update_interval...) that polls the fast entities and sends
        # every write. There is deliberately no update_interval here: the cadence is the one of the controllers.
        cv.Required(CONF_FAST_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        # Optional second modbus_controller (same inverter, slower update_interval) for the slow entities. Without
        # it the slow entities are polled by the fast one.
        cv.Optional(CONF_SLOW_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        # Unit of the power registers that depend on the inverter's "AC power ratio" (register 54: 1 W or 10 W, grid,
        # load...). `auto` (default) reads that register; a number forces the multiplier instead.
        cv.Optional(CONF_INVERTER_FACTOR, default="auto"): inverter_factor,
        # Write with the "write multiple registers" function (0x10, one register) instead of 0x06
        cv.Optional(CONF_USE_WRITE_MULTIPLE, default=True): cv.boolean,
        # After a write, the values read for that register are ignored for this long
        cv.Optional(CONF_WRITE_HOLD, default="5s"): cv.positive_time_period_milliseconds,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    fast = await cg.get_variable(config[CONF_FAST_MODBUS_CONTROLLER_ID])
    cg.add(var.set_fast_controller(fast))
    if CONF_SLOW_MODBUS_CONTROLLER_ID in config:
        slow = await cg.get_variable(config[CONF_SLOW_MODBUS_CONTROLLER_ID])
        cg.add(var.set_slow_controller(slow))
    cg.add(var.set_phase(config[CONF_PHASE]))
    if config[CONF_INVERTER_FACTOR] == "auto":
        cg.add(var.set_inverter_factor_auto())
    else:
        cg.add(var.set_inverter_factor(config[CONF_INVERTER_FACTOR]))
    cg.add(var.set_use_write_multiple(config[CONF_USE_WRITE_MULTIPLE]))
    cg.add(var.set_write_hold(config[CONF_WRITE_HOLD]))


# ---------------------------------------------------------------------------
# Helpers shared by the platforms
# ---------------------------------------------------------------------------

# Every platform block declares `deyelvhy_id:` pointing at the hub declared in the top-level `deyelvhy:` block
# (optional when there is a single hub).
DEYELVHY_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYELVHY_ID): cv.use_id(DeyeLvHyHub),
    }
)


def union_specs(platform):
    """All the keys a platform can carry, over every phase (the first phase that has a key gives its attributes)."""
    out = {}
    for phase in PHASES:
        for key, spec in REGISTERS[phase][platform].items():
            out.setdefault(key, spec)
    return out


def _find_hub_config(hubs, hub_id):
    for conf in hubs or []:
        if conf[CONF_ID].id == hub_id.id:
            return conf
    raise cv.Invalid(f"Deye hub '{hub_id.id}' not found")


def hub_config(hub_id):
    """Configuration of the hub a platform points to (code generation time)."""
    return _find_hub_config(CORE.config.get(DOMAIN), hub_id)


def final_validate_platform(platform):
    """Reject the keys the phase of the hub does not have (final validate time, where CORE.config is not populated
    yet: the hub is read from fv.full_config)."""
    keys = union_specs(platform)

    def _validate(config):
        hub = _find_hub_config(fv.full_config.get().get(DOMAIN), config[CONF_DEYELVHY_ID])
        phase = hub[CONF_PHASE]
        available = REGISTERS[phase][platform]
        for key in config:
            if key in keys and key not in available:
                others = [PHASE_NAMES[p] for p in PHASES if key in REGISTERS[p][platform]]
                raise cv.Invalid(
                    f"'{key}' is not available on a {PHASE_NAMES[phase]} inverter (phase: {phase}). "
                    f"It exists on: {', '.join(others) or 'no phase'}.",
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
        "accuracy_decimals": spec.get("precision"),
    }
    return {n: mapping[n] for n in names if mapping[n] is not None}


def vtype_of(spec):
    """Modbus value type of a spec (a single unsigned register unless the table says otherwise)."""
    return SENSOR_VALUE_TYPE[spec.get("vtype", "U_WORD")]


def transform_args(spec, factor):
    """(add, scale, wrap) of a register -> value conversion; the hub's inverter_factor is folded into `scale`."""
    scale = spec.get("scale", 1.0)
    if spec.get("factor") and factor != "auto":
        scale *= factor
    if spec.get("negate"):
        scale = -scale
    return spec.get("add", 0), scale, bool(spec.get("wrap", False))


def add_register_item(hub, var, spec, slow):
    """Give an entity its register and register it with the controller (through the hub)."""
    cg.add(var.configure(spec["register"], vtype_of(spec)))
    if spec.get("bridge"):
        cg.add(var.set_bridge(True))
    cg.add(hub.add_item(var, slow))


def use_ratio(hub, item, spec, factor):
    """In `auto` mode, a register that depends on the power ratio (register 54) is converted with the ratio the hub reads."""
    if spec.get("factor") and factor == "auto":
        cg.add(item.set_ratio(hub))
        cg.add(hub.need_ratio())


def hub_phase_specs(hub_id, platform):
    """(hub, phase table of the platform, inverter_factor) at code generation time."""
    conf = hub_config(hub_id)
    return REGISTERS[conf[CONF_PHASE]][platform], conf[CONF_INVERTER_FACTOR]
