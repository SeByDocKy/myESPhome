"""Deye single-phase hybrid inverter (SUN-xK-SG0x-LP1 family) over Modbus RTU.

Native ESPHome component: a hub (`deye_mono:`) plus the sensor / binary_sensor / switch / number / select /
text_sensor platforms. It sits on top of the ESPHome `modbus` bus and `modbus_controller` (both declared in the YAML:
the controller polls the inverter and owns the one and only `update_interval`); the entities come from the register
table in registers.py. The total-energy part (`total_daily_energy`, the "yesterday" values) stays in the YAML.

Compared to the plain `modbus_controller` entities, the hub adds:
  - a write that only changes its own bits of a register shared by several switches / selects (read-modify-write on a
    copy of the registers last read), where the stock modbus switch overwrites the whole register;
  - a hold-off after a write, so the value read back by a poll that was already under way does not undo it;
  - the calculated sensors (battery charge / discharge, essential / non-essential load...), which read their own
    registers and are computed in C++ instead of by polling template sensors.
"""

from __future__ import annotations

import esphome.codegen as cg
from esphome.components import modbus_controller
from esphome.components.modbus.helpers import SENSOR_VALUE_TYPE
from esphome.components.modbus_controller import CONF_MODBUS_CONTROLLER_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["modbus_controller"]
DEPENDENCIES = ["modbus_controller"]
MULTI_CONF = True

DOMAIN = "deye_mono"

deye_mono_ns = cg.esphome_ns.namespace("deye_mono")
DeyeMonoHub = deye_mono_ns.class_("DeyeMonoHub", cg.Component)

CONF_DEYE_MONO_ID = "deye_mono_id"
CONF_INVERTER_FACTOR = "inverter_factor"
CONF_USE_WRITE_MULTIPLE = "use_write_multiple"
CONF_WRITE_HOLD = "write_hold"

ModbusController = modbus_controller.ModbusController

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeyeMonoHub),
        # The modbus_controller (address of the inverter, update_interval, retries...) that polls the inverter.
        # There is deliberately no update_interval here: the cadence is the one of the controller.
        cv.Required(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        # Multiplier of every power value of the inverter (1 for a single inverter, N for N inverters in
        # parallel on the same CT clamp...). It was `${inverter_factor}` in the YAML.
        cv.Optional(CONF_INVERTER_FACTOR, default=1.0): cv.positive_float,
        # Write with the "write multiple registers" function (0x10, one register) instead of 0x06
        cv.Optional(CONF_USE_WRITE_MULTIPLE, default=True): cv.boolean,
        # After a write, the values read for that register are ignored for this long (see the module docstring)
        cv.Optional(CONF_WRITE_HOLD, default="5s"): cv.positive_time_period_milliseconds,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    controller = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_controller(controller))
    cg.add(var.set_inverter_factor(config[CONF_INVERTER_FACTOR]))
    cg.add(var.set_use_write_multiple(config[CONF_USE_WRITE_MULTIPLE]))
    cg.add(var.set_write_hold(config[CONF_WRITE_HOLD]))


# ---------------------------------------------------------------------------
# Helpers shared by the platforms
# ---------------------------------------------------------------------------

# Every platform block declares `deye_mono_id:` pointing at the hub declared in the top-level `deye_mono:` block
# (optional when there is a single hub).
DEYE_MONO_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DEYE_MONO_ID): cv.use_id(DeyeMonoHub),
    }
)


def hub_config(hub_id):
    """Configuration of the hub a platform points to (code generation time)."""
    for conf in CORE.config.get(DOMAIN) or []:
        if conf[CONF_ID].id == hub_id.id:
            return conf
    raise cv.Invalid(f"Deye hub '{hub_id.id}' not found")


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
    if spec.get("factor"):
        scale *= factor
    if spec.get("negate"):
        scale = -scale
    return spec.get("add", 0), scale, bool(spec.get("wrap", False))


def add_register_item(hub, var, spec):
    """Give an entity its register and register it with the controller (through the hub)."""
    cg.add(var.configure(spec["register"], vtype_of(spec)))
    if spec.get("bridge"):
        cg.add(var.set_bridge(True))
    cg.add(hub.add_item(var))
