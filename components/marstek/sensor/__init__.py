import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.modbustcp_controller import SENSOR_VALUE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import (
    CONF_MARSTEK_ID,
    CONF_MODEL,
    MARSTEK_PLATFORM_SCHEMA,
    MODELS,
    entity_kwargs,
    final_validate_hub_model,
    hub_config,
    marstek_ns,
    skip_updates,
    union_specs,
)
from ..registers import MODEL_NAMES, REGISTERS

DEPENDENCIES = ["marstek"]

MarstekSensor = marstek_ns.class_("MarstekSensor", sensor.Sensor, cg.Component)
MarstekCalcSensor = marstek_ns.class_("MarstekCalcSensor", sensor.Sensor, cg.Component)
CalcMode = marstek_ns.enum("CalcMode", True)

MODES = {
    "round_trip": CalcMode.ROUND_TRIP,
    "conversion": CalcMode.CONVERSION,
    "stored_energy": CalcMode.STORED_ENERGY,
    "cycles": CalcMode.CYCLES,
    "sum": CalcMode.SUM,
}
# Order of the sources, as expected by MarstekCalcSensor::recompute_()
SOURCE_ORDER = {
    "round_trip": ("charge", "discharge"),
    "conversion": ("battery_power", "ac_power"),
    "stored_energy": ("soc", "capacity"),
    "cycles": ("discharge", "capacity"),
}

SPECS = union_specs("sensor")


def _schema(spec):
    cls = MarstekCalcSensor if spec["kind"] == "calc" else MarstekSensor
    return sensor.sensor_schema(
        cls,
        **entity_kwargs(
            spec,
            "unit_of_measurement",
            "icon",
            "entity_category",
            "device_class",
            "state_class",
            "accuracy_decimals",
        ),
    )


# ---------------------------------------------------------------------------
# dc_channels: / ac: / battery: grouping
#
# This is a readability layer over the flat register keys of REGISTERS[<model>]["sensor"]: every
# nested field below maps to exactly one flat key (dc_channels.pv<n>.<field> -> "mppt<n+1>_<field>",
# ac.<field> -> "ac_<field>", battery.<field> -> "battery_<field>", battery.packs.pack<n>.soc ->
# "battery_soc_<n+1>", battery.packs.pack<n>.cells[<c>] -> "battery_<n+1>_cell_<c+1>_voltage"). The
# mapping itself does not depend on the selected model; per-model availability is enforced the usual
# way by FINAL_VALIDATE_SCHEMA below, which knows how to point at the nested path on error.
#
# Special case: "e_v3" has a single battery pack with no indexed "battery_soc_1" register (only the
# plain "battery_soc"), so pack0.soc resolves to "battery_soc" on any model that lacks
# "battery_soc_1" -- the nested YAML still reads `battery: packs: pack0: soc: ...` on every model.
# ---------------------------------------------------------------------------

MAX_PV_CHANNELS = 4  # mppt1_.. mppt4_ (models a, d)
PV_FIELDS = ("voltage", "current", "power")

MAX_BATTERY_PACKS = 6  # battery_soc_1 .. battery_soc_6 / battery_1_cell_.. .. battery_6_cell_.. (models a, d)
MAX_CELLS_PER_PACK = 16  # battery_<n>_cell_1_.. _16_voltage (models d, e_v3)

AC_FIELDS = {
    "voltage": "ac_voltage",
    "current": "ac_current",
    "frequency": "ac_frequency",
    "power": "ac_power",
    "offgrid_voltage": "ac_offgrid_voltage",
    "offgrid_current": "ac_offgrid_current",
    "offgrid_power": "ac_offgrid_power",
}

BATTERY_FIELDS = {
    "voltage": "battery_voltage",
    "current": "battery_current",
    "soc": "battery_soc",
    "total_energy": "battery_total_energy",
    "cycle_count": "battery_cycle_count",
    "cycle_count_calc": "battery_cycle_count_calc",
}

GROUP_KEYS = ("dc_channels", "ac", "battery")


def _pv_key(channel, field):
    return f"mppt{channel + 1}_{field}"


def _pack_soc_key(pack):
    return f"battery_soc_{pack + 1}"


def _cell_key(pack, cell):
    return f"battery_{pack + 1}_cell_{cell + 1}_voltage"


def _group_schema(fields):
    """fields: {nested name: flat register key}. Keeps only the fields that exist on at least one model."""
    return cv.Schema({cv.Optional(name): _schema(SPECS[flat]) for name, flat in fields.items() if flat in SPECS})


# One representative schema per nested shape, reused across every pv<n> / pack<n> / cell: the register
# differs (and so does its per-model availability), but the entity schema itself (unit, device_class,
# icon...) is the same shape for every channel/pack/cell of a given field.
_PV_CHANNEL_SCHEMA = cv.Schema(
    {cv.Optional(field): _schema(SPECS[_pv_key(0, field)]) for field in PV_FIELDS if _pv_key(0, field) in SPECS}
)
DC_CHANNELS_SCHEMA = cv.Schema({cv.Optional(f"pv{i}"): _PV_CHANNEL_SCHEMA for i in range(MAX_PV_CHANNELS)})

_PACK_SOC_SCHEMA = _schema(SPECS["battery_soc"])
_CELL_SCHEMA = _schema(SPECS["battery_1_cell_1_voltage"])
_PACK_SCHEMA = cv.Schema(
    {
        cv.Optional("soc"): _PACK_SOC_SCHEMA,
        cv.Optional("cells"): cv.All(cv.ensure_list(_CELL_SCHEMA), cv.Length(max=MAX_CELLS_PER_PACK)),
    }
)
PACKS_SCHEMA = cv.Schema({cv.Optional(f"pack{i}"): _PACK_SCHEMA for i in range(MAX_BATTERY_PACKS)})

AC_SCHEMA = _group_schema(AC_FIELDS)
BATTERY_SCHEMA = _group_schema(BATTERY_FIELDS).extend({cv.Optional("packs"): PACKS_SCHEMA})

# Every union key not absorbed by dc_channels: / ac: / battery: above stays a flat, top level key
# (global diagnostics: versions, wifi, internal temperatures, efficiencies, energy totals...).
_GROUPED_FLAT_KEYS = set(AC_FIELDS.values()) | set(BATTERY_FIELDS.values())
_GROUPED_FLAT_KEYS |= {_pv_key(i, f) for i in range(MAX_PV_CHANNELS) for f in PV_FIELDS}
_GROUPED_FLAT_KEYS |= {_pack_soc_key(i) for i in range(MAX_BATTERY_PACKS)}
_GROUPED_FLAT_KEYS |= {_cell_key(p, c) for p in range(MAX_BATTERY_PACKS) for c in range(MAX_CELLS_PER_PACK)}

FLAT_SPECS = {key: spec for key, spec in SPECS.items() if key not in _GROUPED_FLAT_KEYS}

CONFIG_SCHEMA = MARSTEK_PLATFORM_SCHEMA.extend(
    {cv.Optional(key): _schema(spec) for key, spec in FLAT_SPECS.items()}
).extend(
    {
        cv.Optional("dc_channels"): DC_CHANNELS_SCHEMA,
        cv.Optional("ac"): AC_SCHEMA,
        cv.Optional("battery"): BATTERY_SCHEMA,
    }
)


def _entities(config, available):
    """Yield (flat register key, error-message path, entity config) for every sensor the user
    configured, flat or nested under dc_channels: / ac: / battery:. `available` is
    REGISTERS[<model>]["sensor"]: it only affects the pack0/soc fallback (see module docstring); it
    is never used to filter entries out here, that is FINAL_VALIDATE_SCHEMA's and to_code's job."""
    for key, conf in config.items():
        if key in (CONF_ID, CONF_MARSTEK_ID) or key in GROUP_KEYS:
            continue
        yield key, [key], conf

    for i in range(MAX_PV_CHANNELS):
        pv_conf = (config.get("dc_channels") or {}).get(f"pv{i}")
        if not pv_conf:
            continue
        for field, conf in pv_conf.items():
            yield _pv_key(i, field), ["dc_channels", f"pv{i}", field], conf

    for field, conf in (config.get("ac") or {}).items():
        yield AC_FIELDS[field], ["ac", field], conf

    battery = config.get("battery") or {}
    for field, conf in battery.items():
        if field != "packs":
            yield BATTERY_FIELDS[field], ["battery", field], conf

    packs = battery.get("packs") or {}
    for i in range(MAX_BATTERY_PACKS):
        pack_conf = packs.get(f"pack{i}")
        if not pack_conf:
            continue
        if "soc" in pack_conf:
            key = _pack_soc_key(i)
            # Fallback to the aggregate "battery_soc" register only for pack0 of a model that has no
            # indexed "battery_soc_1" but does have at least one of its own cell registers (e_v3: a
            # single physical pack). A model with no per-pack data at all (e_v12) has no pack0 either,
            # indexed or not: "battery.soc" is the only battery SOC entity it offers.
            if key not in available and i == 0 and "battery_soc" in available and _cell_key(0, 0) in available:
                key = "battery_soc"
            yield key, ["battery", "packs", f"pack{i}", "soc"], pack_conf["soc"]
        for c, cell_conf in enumerate(pack_conf.get("cells") or []):
            yield _cell_key(i, c), ["battery", "packs", f"pack{i}", "cells", c], cell_conf


def _final_validate(config):
    model = final_validate_hub_model(config[CONF_MARSTEK_ID])
    available = REGISTERS[model]["sensor"]
    for key, path, _conf in _entities(config, available):
        if key not in SPECS or key in available:
            continue
        others = [MODEL_NAMES[m] for m in MODELS if key in REGISTERS[m]["sensor"]]
        raise cv.Invalid(
            f"'{'.'.join(str(p) for p in path)}' is not available on the Marstek {MODEL_NAMES[model]} "
            f"(model: {model}). It exists on: {', '.join(others) or 'no model'}.",
            path=path,
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MARSTEK_ID])
    hub_conf = hub_config(config[CONF_MARSTEK_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["sensor"]

    for key, _path, conf in _entities(config, available):
        if key not in available:
            continue
        spec = available[key]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)

        if spec["kind"] == "calc":
            order = SOURCE_ORDER.get(spec["mode"]) or sorted(spec["deps"])
            cg.add(var.set_mode(MODES[spec["mode"]]))
            cg.add(var.set_dep_count(len(order)))
            for index, alias in enumerate(order):
                dep = spec["deps"][alias]
                cg.add(
                    var.configure_dep(
                        index,
                        dep["register"],
                        dep["count"],
                        SENSOR_VALUE_TYPE[dep["vtype"]],
                        dep["scale"],
                        skip_updates(dep, hub_conf),
                    )
                )
                cg.add(hub.add_sensor_item(var.get_dep(index)))
            continue

        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                skip_updates(spec, hub_conf),
            )
        )
        cg.add(var.set_scale(spec.get("scale", 1)))
        if spec.get("transform") == "ems_version":
            cg.add(var.set_ems_transform(True))
        cg.add(hub.add_sensor_item(var))
