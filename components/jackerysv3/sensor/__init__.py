import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
    UNIT_WATT,
)

from .. import CONF_JACKERYSV3_ID, MAX_PLUGS, MAX_PV_CHANNELS, JackerySV3Hub

DEPENDENCIES = ["jackerysv3"]

# Structure (same philosophy as this author's marstek / zensdk / hms components): the sensors are grouped under
# `dc_channels:` (solar input), `ac:` (grid-tied port, AC socket, home load), `battery:` plus, specific to this
# battery, `energy_flows:`, `ct:` (smart meter) and `plugs:` (smart plugs). PV channels and plugs are
# 0-indexed: `pv0` .. `pv3`, `plug0` .. `plug9`.
#
# Every nested field maps to one "flat key" known by the C++ decoder (jackery_state.cpp, SENSOR_KINDS). The
# flat keys are not part of the YAML.

CONF_DC_CHANNELS = "dc_channels"
CONF_AC = "ac"
CONF_BATTERY = "battery"
CONF_ENERGY_FLOWS = "energy_flows"
CONF_CT = "ct"
CONF_PLUGS = "plugs"
CONF_STATUS_CODE = "status_code"
CONF_WORK_MODE_CODE = "work_mode_code"


def _power(icon="mdi:flash"):
    return dict(
        unit_of_measurement=UNIT_WATT,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon=icon,
    )


def _energy(icon="mdi:lightning-bolt"):
    # Counters of the battery (raw value x 0.01 kWh)
    return dict(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        accuracy_decimals=2,
        icon=icon,
    )


def _soc(icon):
    return dict(
        unit_of_measurement=UNIT_PERCENT,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon=icon,
    )


def _code(icon):
    return dict(
        accuracy_decimals=0,
        icon=icon,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


# ---- flat key -> schema attributes ---------------------------------------------------------------------------

SPECS = {
    # device level
    "status_code": _code("mdi:state-machine"),
    "work_mode_code": _code("mdi:cog-outline"),
    # dc_channels
    "solar_power": _power("mdi:solar-power"),
    "solar_energy": _energy("mdi:solar-power"),
    "pv_power": _power("mdi:solar-panel"),
    "pv_energy": _energy("mdi:solar-panel"),
    # ac
    "ac_grid_input_power": _power("mdi:transmission-tower-export"),
    "ac_grid_output_power": _power("mdi:transmission-tower-import"),
    "ac_grid_input_energy": _energy("mdi:transmission-tower-export"),
    "ac_grid_output_energy": _energy("mdi:transmission-tower-import"),
    "ac_grid_power": _power("mdi:transmission-tower"),
    "ac_home_power": _power("mdi:home-lightning-bolt-outline"),
    "ac_other_load_power": _power("mdi:home-lightning-bolt-outline"),
    "ac_max_feed_in_power": _power("mdi:speedometer"),
    "ac_socket_power": _power("mdi:power-plug"),
    "ac_socket_input_power": _power("mdi:power-plug-battery"),
    "ac_socket_input_energy": _energy("mdi:power-plug-battery"),
    "ac_socket_output_energy": _energy("mdi:power-plug"),
    # battery
    "battery_soc": _soc("mdi:battery-50"),
    "battery_average_soc": _soc("mdi:battery-70"),
    "battery_temperature": dict(
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=1,
        icon="mdi:thermometer",
    ),
    "battery_pack_count": dict(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:battery-plus-variant",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "battery_charge_power": _power("mdi:battery-arrow-up"),
    "battery_discharge_power": _power("mdi:battery-arrow-down"),
    "battery_net_power": _power("mdi:battery-charging"),
    "battery_charge_energy": _energy("mdi:battery-plus"),
    "battery_discharge_energy": _energy("mdi:battery-minus"),
    # energy_flows (battery counters, kWh)
    "pv_to_battery_energy": _energy("mdi:solar-power"),
    "pv_to_socket_energy": _energy("mdi:solar-power"),
    "pv_to_grid_energy": _energy("mdi:transmission-tower-import"),
    "grid_to_socket_energy": _energy("mdi:transmission-tower-export"),
    "grid_to_battery_energy": _energy("mdi:transmission-tower-export"),
    "battery_to_socket_energy": _energy("mdi:battery-minus"),
    "battery_to_grid_energy": _energy("mdi:battery-minus"),
    "socket_to_battery_energy": _energy("mdi:power-plug-battery"),
    "socket_to_grid_energy": _energy("mdi:power-plug"),
    # ct (smart meter)
    "ct_forward_power": _power("mdi:current-ac"),
    "ct_reverse_power": _power("mdi:current-ac"),
    "ct_forward_energy": _energy("mdi:current-ac"),
    "ct_reverse_energy": _energy("mdi:current-ac"),
    "ct_phase_a_forward_power": _power("mdi:current-ac"),
    "ct_phase_b_forward_power": _power("mdi:current-ac"),
    "ct_phase_c_forward_power": _power("mdi:current-ac"),
    "ct_phase_a_reverse_power": _power("mdi:current-ac"),
    "ct_phase_b_reverse_power": _power("mdi:current-ac"),
    "ct_phase_c_reverse_power": _power("mdi:current-ac"),
    # plugs
    "plug_power": _power("mdi:power-socket-eu"),
    "plug_energy": _energy("mdi:lightning-bolt"),
}

# ---- nested name -> flat key ---------------------------------------------------------------------------------

DC_FIELDS = {"power": "solar_power", "energy": "solar_energy"}
PV_FIELDS = {"power": "pv_power", "energy": "pv_energy"}
AC_FIELDS = {
    "grid_power": "ac_grid_power",
    "grid_input_power": "ac_grid_input_power",
    "grid_output_power": "ac_grid_output_power",
    "grid_input_energy": "ac_grid_input_energy",
    "grid_output_energy": "ac_grid_output_energy",
    "home_power": "ac_home_power",
    "other_load_power": "ac_other_load_power",
    "max_feed_in_power": "ac_max_feed_in_power",
    "socket_power": "ac_socket_power",
    "socket_input_power": "ac_socket_input_power",
    "socket_input_energy": "ac_socket_input_energy",
    "socket_output_energy": "ac_socket_output_energy",
}
BATTERY_FIELDS = {
    "soc": "battery_soc",
    "average_soc": "battery_average_soc",
    "temperature": "battery_temperature",
    "pack_count": "battery_pack_count",
    "net_power": "battery_net_power",
    "charge_power": "battery_charge_power",
    "discharge_power": "battery_discharge_power",
    "charge_energy": "battery_charge_energy",
    "discharge_energy": "battery_discharge_energy",
}
ENERGY_FLOW_FIELDS = {
    "pv_to_battery": "pv_to_battery_energy",
    "pv_to_socket": "pv_to_socket_energy",
    "pv_to_grid": "pv_to_grid_energy",
    "grid_to_battery": "grid_to_battery_energy",
    "grid_to_socket": "grid_to_socket_energy",
    "battery_to_socket": "battery_to_socket_energy",
    "battery_to_grid": "battery_to_grid_energy",
    "socket_to_battery": "socket_to_battery_energy",
    "socket_to_grid": "socket_to_grid_energy",
}
CT_FIELDS = {
    "forward_power": "ct_forward_power",
    "reverse_power": "ct_reverse_power",
    "forward_energy": "ct_forward_energy",
    "reverse_energy": "ct_reverse_energy",
    "phase_a_forward_power": "ct_phase_a_forward_power",
    "phase_b_forward_power": "ct_phase_b_forward_power",
    "phase_c_forward_power": "ct_phase_c_forward_power",
    "phase_a_reverse_power": "ct_phase_a_reverse_power",
    "phase_b_reverse_power": "ct_phase_b_reverse_power",
    "phase_c_reverse_power": "ct_phase_c_reverse_power",
}
PLUG_FIELDS = {"power": "plug_power", "energy": "plug_energy"}
FLAT_FIELDS = {CONF_STATUS_CODE: "status_code", CONF_WORK_MODE_CODE: "work_mode_code"}


def _schema(flat):
    return sensor.sensor_schema(**SPECS[flat])


def _fields_schema(fields):
    return cv.Schema({cv.Optional(name): _schema(flat) for name, flat in fields.items()})


def _indexed_schema(prefix, count, fields):
    one = _fields_schema(fields)
    return cv.Schema({cv.Optional(f"{prefix}{i}"): one for i in range(count)})


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_JACKERYSV3_ID): cv.use_id(JackerySV3Hub),
        **{cv.Optional(name): _schema(flat) for name, flat in FLAT_FIELDS.items()},
        cv.Optional(CONF_DC_CHANNELS): _indexed_schema("pv", MAX_PV_CHANNELS, PV_FIELDS).extend(
            {cv.Optional(name): _schema(flat) for name, flat in DC_FIELDS.items()}
        ),
        cv.Optional(CONF_AC): _fields_schema(AC_FIELDS),
        cv.Optional(CONF_BATTERY): _fields_schema(BATTERY_FIELDS),
        cv.Optional(CONF_ENERGY_FLOWS): _fields_schema(ENERGY_FLOW_FIELDS),
        cv.Optional(CONF_CT): _fields_schema(CT_FIELDS),
        cv.Optional(CONF_PLUGS): _indexed_schema("plug", MAX_PLUGS, PLUG_FIELDS),
    }
)


def _entities(config):
    """Yield (flat key, index, entity config) for every configured sensor."""
    for name, flat in FLAT_FIELDS.items():
        if name in config:
            yield flat, 0, config[name]

    dc = config.get(CONF_DC_CHANNELS) or {}
    for name, flat in DC_FIELDS.items():
        if name in dc:
            yield flat, 0, dc[name]
    for i in range(MAX_PV_CHANNELS):
        for name, flat in PV_FIELDS.items():
            if name in (dc.get(f"pv{i}") or {}):
                yield flat, i, dc[f"pv{i}"][name]

    for group, fields in (
        (CONF_AC, AC_FIELDS),
        (CONF_BATTERY, BATTERY_FIELDS),
        (CONF_ENERGY_FLOWS, ENERGY_FLOW_FIELDS),
        (CONF_CT, CT_FIELDS),
    ):
        conf = config.get(group) or {}
        for name, flat in fields.items():
            if name in conf:
                yield flat, 0, conf[name]

    plugs = config.get(CONF_PLUGS) or {}
    for i in range(MAX_PLUGS):
        for name, flat in PLUG_FIELDS.items():
            if name in (plugs.get(f"plug{i}") or {}):
                yield flat, i, plugs[f"plug{i}"][name]


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JACKERYSV3_ID])
    for flat, index, conf in _entities(config):
        var = await sensor.new_sensor(conf)
        cg.add(hub.add_sensor(flat, index, var))
