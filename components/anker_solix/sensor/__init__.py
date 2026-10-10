import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import (
    CONF_ANKER_SOLIX_ID,
    CONF_MODEL,
    ANKER_SOLIX_PLATFORM_SCHEMA,
    MODELS,
    anker_solix_ns,
    entity_kwargs,
    final_validate_hub_model,
    hub_config,
    is_slow,
    union_specs,
)
from ..registers import MODEL_NAMES, REGISTERS

DEPENDENCIES = ["anker_solix"]

AnkerSolixSensor = anker_solix_ns.class_("AnkerSolixSensor", sensor.Sensor, cg.Component)
AnkerSolixCalcSensor = anker_solix_ns.class_("AnkerSolixCalcSensor", sensor.Sensor, cg.Component)
SplitMode = anker_solix_ns.enum("SplitMode", True)

SPLITS = {
    "negative_only": SplitMode.NEGATIVE_ONLY,
    "positive_only": SplitMode.POSITIVE_ONLY,
}

SPECS = union_specs("sensor")


def _schema(spec):
    cls = AnkerSolixCalcSensor if spec["kind"] == "calc" else AnkerSolixSensor
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
# dc_channels: / dc_channels: / ac: / battery: grouping
#
# This is a readability layer over the flat register keys of REGISTERS[<model>]["sensor"] (the keys of the
# official integration): every nested field below maps to exactly one flat key. The Solarbank reports one
# aggregated PV power rather than one value per MPPT, so dc_channels: holds those aggregated PV values (and there is
# no packs: block: the battery is reported as a whole). Everything else (diagnostic masks) stays flat.
# Per-model availability is enforced by FINAL_VALIDATE_SCHEMA below, which points at the nested path on error.
# ---------------------------------------------------------------------------

AC_FIELDS = {
    "output_power": "ac_grid_output_power",
    "load_power": "load_power",
    "grid_import_power": "grid_import_power",
    "grid_export_power": "grid_export_power",
}

DC_CHANNELS_FIELDS = {
    "pv_power": "pv_power",
    "pcs_pv_power": "pcs_pv_power",
    "third_party_pv_power": "third_party_pv_power",
    "pv_total_generation": "pv_total_generation",
}

BATTERY_FIELDS = {
    "soc": "battery_soc",
    "charging_power": "battery_charging_power",
    "discharging_power": "battery_discharging_power",
    "rated_energy": "rated_energy",
    "total_charging_energy": "cumulative_charge_energy",
    "total_discharging_energy": "cumulative_discharge_energy",
    "max_charging_power": "max_charge_power",
    "max_discharging_power": "max_discharge_power",
}

GROUP_KEYS = ("dc_channels", "ac", "battery")


def _group_schema(fields):
    """fields: {nested name: flat register key}. Keeps only the fields that exist on at least one model."""
    return cv.Schema({cv.Optional(name): _schema(SPECS[flat]) for name, flat in fields.items() if flat in SPECS})


DC_CHANNELS_SCHEMA = _group_schema(DC_CHANNELS_FIELDS)
AC_SCHEMA = _group_schema(AC_FIELDS)
BATTERY_SCHEMA = _group_schema(BATTERY_FIELDS)

# Every union key not absorbed by ac: / battery: above stays a flat, top level key
_GROUPED_FLAT_KEYS = (
    set(DC_CHANNELS_FIELDS.values()) | set(AC_FIELDS.values()) | set(BATTERY_FIELDS.values())
)
FLAT_SPECS = {key: spec for key, spec in SPECS.items() if key not in _GROUPED_FLAT_KEYS}

CONFIG_SCHEMA = ANKER_SOLIX_PLATFORM_SCHEMA.extend(
    {cv.Optional(key): _schema(spec) for key, spec in FLAT_SPECS.items()}
).extend(
    {
        cv.Optional("dc_channels"): DC_CHANNELS_SCHEMA,
        cv.Optional("ac"): AC_SCHEMA,
        cv.Optional("battery"): BATTERY_SCHEMA,
    }
)


def _entities(config):
    """Yield (flat register key, error-message path, entity config) for every sensor the user configured, flat or
    nested under ac: / battery:."""
    for key, conf in config.items():
        if key in (CONF_ID, CONF_ANKER_SOLIX_ID) or key in GROUP_KEYS:
            continue
        yield key, [key], conf

    for field, conf in (config.get("dc_channels") or {}).items():
        yield DC_CHANNELS_FIELDS[field], ["dc_channels", field], conf

    for field, conf in (config.get("ac") or {}).items():
        yield AC_FIELDS[field], ["ac", field], conf

    for field, conf in (config.get("battery") or {}).items():
        yield BATTERY_FIELDS[field], ["battery", field], conf


def _final_validate(config):
    model = final_validate_hub_model(config[CONF_ANKER_SOLIX_ID])
    available = REGISTERS[model]["sensor"]
    for key, path, _conf in _entities(config):
        if key not in SPECS or key in available:
            continue
        others = [MODEL_NAMES[m] for m in MODELS if key in REGISTERS[m]["sensor"]]
        raise cv.Invalid(
            f"'{'.'.join(str(p) for p in path)}' is not available on the {MODEL_NAMES[model]} "
            f"(model: {model}). It exists on: {', '.join(others) or 'no model'}.",
            path=path,
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ANKER_SOLIX_ID])
    hub_conf = hub_config(config[CONF_ANKER_SOLIX_ID])
    available = REGISTERS[hub_conf[CONF_MODEL]]["sensor"]

    for key, _path, conf in _entities(config):
        if key not in available:
            continue
        spec = available[key]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)

        if spec["kind"] == "calc":
            # Sum of registers it reads itself: the source sensors do not have to be declared
            deps = list(spec["deps"].values())
            cg.add(var.set_dep_count(len(deps)))
            for index, dep in enumerate(deps):
                cg.add(
                    var.configure_dep(
                        index,
                        dep["register"],
                        dep["count"],
                        SENSOR_VALUE_TYPE[dep["vtype"]],
                        MODBUS_REGISTER_TYPE[dep["rtype"]],
                    )
                )
                cg.add(hub.add_item(var.get_dep(index), is_slow(dep)))
            continue

        cg.add(
            var.configure(
                spec["register"],
                spec["count"],
                SENSOR_VALUE_TYPE[spec["vtype"]],
                MODBUS_REGISTER_TYPE[spec["rtype"]],
            )
        )
        cg.add(var.set_scale(spec.get("scale", 1)))
        if "split" in spec:
            cg.add(var.set_split(SPLITS[spec["split"]]))
        if spec.get("own_range"):
            cg.add(var.set_own_range(True))
        cg.add(hub.add_item(var, is_slow(spec)))
