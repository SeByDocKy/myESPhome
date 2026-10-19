"""Native ESPHome client for SunSpec devices over Modbus (RTU or TCP).

SunSpec devices (inverters, batteries, meters, ...) describe themselves: after a
"SunS" marker, the device exposes a chain of *models*, each introduced by a
two-register header (model id, length), ended by the id 0xFFFF.

This component is a client on the core ``modbus`` bus. The transport is chosen
by the bus ``uart_id``: a real UART (RS485) or ``tcp_uart`` + ``modbus_tcp_uart``
for Modbus TCP, exactly like the ``anker_solix`` component.

Operation: the device is scanned first (the list of models is published by the
``text_sensor`` platform). Then the models of the ``models`` wish list that the
device really exposes are read once, the sensors of the points the device
implements are created all at once at runtime, and Home Assistant is asked to
re-read the entity list a single time (the API client is asked to reconnect).
The sensors are then refreshed every ``update_interval``.

Enum, bitfield and string points become text sensors (the enum symbol, the names of the set bits, the
string). Sensor slots must exist at compile time (ESPHome entity containers are fixed
size), so they are reserved here from the ``models`` list and ``max_modules``.

NOTE: this relies on ``esphome.core.entity_helpers`` which is an internal
ESPHome module (packed entity fields, ESPHome >= 2026.8).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import modbus
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.core.entity_helpers import (
    register_device_class,
    register_icon,
    register_unit_of_measurement,
)
from esphome.types import ConfigType

from .sunspec_models_info import MODEL_INFO, POINT_KINDS

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["modbus", "network", "sensor", "text_sensor", "binary_sensor", "number", "select"]
# Several devices on the same bus (or several buses) are possible: one block per device.
MULTI_CONF = True

sunspec_client_ns = cg.esphome_ns.namespace("sunspec_client")
SunSpecClient = sunspec_client_ns.class_(
    "SunSpecClient", cg.Component, modbus.ModbusClientDevice
)

CONF_SUNSPEC_CLIENT_ID = "sunspec_client_id"
CONF_MODELS = "models"
CONF_NAME_PREFIX = "name_prefix"
CONF_TRIM_NAMING = "trim_naming"
CONF_STARTUP_DELAY = "startup_delay"
CONF_SCAN_RETRY_INTERVAL = "scan_retry_interval"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_MAX_MODULES = "max_modules"
CONF_BITFIELD_BINARY_SENSORS = "bitfield_binary_sensors"
CONF_WAIT_FOR_NETWORK = "wait_for_network"
CONF_CONTROLS = "controls"

# Home Assistant device class of each kind of point (PointKind enumerators of the generated tables)
DEVICE_CLASSES = {
    "CURRENT": "current",
    "VOLTAGE": "voltage",
    "POWER": "power",
    "ENERGY": "energy",
    "APPARENT_POWER": "apparent_power",
    "REACTIVE_POWER": "reactive_power",
    "FREQUENCY": "frequency",
    "TEMPERATURE": "temperature",
}

# Icons, same conventions as the other components: one per kind of point, then % / status / alarms.
# Slots follow the PointKind order of the generated tables, then ICON_PERCENT, ICON_INFO, ICON_ALERT.
ICONS = {
    "CURRENT": "mdi:current-ac",
    "VOLTAGE": "mdi:sine-wave",
    "POWER": "mdi:power",
    "ENERGY": "mdi:counter",
    "APPARENT_POWER": "mdi:flash",
    "REACTIVE_POWER": "mdi:flash-outline",
    "FREQUENCY": "mdi:metronome",
    "TEMPERATURE": "mdi:thermometer",
}
ICON_PERCENT = "mdi:percent"
ICON_INFO = "mdi:information-outline"
ICON_ALERT = "mdi:alert-circle-outline"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SunSpecClient),
            # Wish list of SunSpec model ids (compile time). The entities created after
            # the scan are the intersection of this list and what the device exposes.
            cv.Optional(CONF_MODELS, default=[]): cv.ensure_list(
                cv.one_of(*MODEL_INFO, int=True)
            ),
            # Instances of a repeating group (e.g. the MPPT inputs of model 160) that get entities.
            cv.Optional(CONF_MAX_MODULES, default=4): cv.int_range(min=1, max=16),
            # Also create one binary_sensor per named bit of the bitfield points (events, states).
            cv.Optional(CONF_BITFIELD_BINARY_SENSORS, default=False): cv.boolean,
            # Models whose writable points (access RW in the SunSpec definition) become number / select
            # controls instead of read-only sensors. Writes act on the device: off by default, opt in per model.
            cv.Optional(CONF_CONTROLS, default=[]): cv.ensure_list(
                cv.one_of(*MODEL_INFO, int=True)
            ),
            # Wait for the network (WiFi, Ethernet) before the first scan. Default: yes for Modbus TCP
            # (tcp_uart + modbus_tcp_uart), no for a serial line (RS485), which does not depend on it.
            cv.Optional(CONF_WAIT_FOR_NETWORK): cv.boolean,
            # Refresh period of the sensors.
            cv.Optional(
                CONF_UPDATE_INTERVAL, default="10s"
            ): cv.positive_time_period_milliseconds,
            # Prefix of the name of every entity created by this component.
            cv.Optional(CONF_NAME_PREFIX, default=""): cv.string,
            # Entity names in lower case with "_" between the words: spaces and special characters are removed
            # ("Phase A current (W)" -> "phase_a_current_w"), prefix included.
            cv.Optional(CONF_TRIM_NAMING, default=False): cv.boolean,
            # Wait before the first request, so that the link (WiFi / TCP) is up.
            cv.Optional(
                CONF_STARTUP_DELAY, default="5s"
            ): cv.positive_time_period_milliseconds,
            # Wait before scanning again after a failed scan.
            cv.Optional(
                CONF_SCAN_RETRY_INTERVAL, default="30s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(modbus.modbus_device_schema(0x01))
)


def _validate_models(config: ConfigType) -> ConfigType:
    if len(set(config[CONF_MODELS])) != len(config[CONF_MODELS]):
        raise cv.Invalid(f"'{CONF_MODELS}' contains duplicated ids")
    for model in config[CONF_CONTROLS]:
        if model not in config[CONF_MODELS]:
            raise cv.Invalid(f"'{CONF_CONTROLS}': model {model} is not in '{CONF_MODELS}'")
        info = MODEL_INFO[model]
        if info["rw_numbers"] + info["rw_selects"] == 0:
            raise cv.Invalid(f"'{CONF_CONTROLS}': model {model} has no writable point")
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_models)


def uses_modbus_tcp(full_config: ConfigType, modbus_id) -> bool:
    """True when the modbus bus `modbus_id` sits on a modbus_tcp_uart (serial over TCP) instead of a UART."""
    for bus in full_config.get("modbus", []):
        if bus.get(CONF_ID) == modbus_id:
            uart_id = bus.get("uart_id")
            return any(
                dev.get(CONF_ID) == uart_id
                for dev in full_config.get("modbus_tcp_uart", [])
            )
    return False


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("sunspec_client", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Sets the parent bus and the unit address (the class is itself a Modbus client device).
    await modbus.register_modbus_client_device(var, config)

    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY]))
    cg.add(var.set_scan_retry_interval(config[CONF_SCAN_RETRY_INTERVAL]))
    cg.add(var.set_name_prefix(config[CONF_NAME_PREFIX]))
    cg.add(var.set_trim_naming(config[CONF_TRIM_NAMING]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_max_modules(config[CONF_MAX_MODULES]))
    cg.add(var.set_bitfield_binary_sensors(config[CONF_BITFIELD_BINARY_SENSORS]))
    wait_for_network = config.get(CONF_WAIT_FOR_NETWORK)
    if wait_for_network is None:
        wait_for_network = uses_modbus_tcp(CORE.config, config[modbus.CONF_MODBUS_ID])
    cg.add(var.set_wait_for_network(wait_for_network))
    number_slots = 0
    select_slots = 0
    for model in config[CONF_CONTROLS]:
        cg.add(var.add_control_model(model))
        number_slots += MODEL_INFO[model]["rw_numbers"]
        select_slots += MODEL_INFO[model]["rw_selects"]
    CORE.platform_counts["number"] += number_slots
    CORE.platform_counts["select"] += select_slots

    slots = 0
    text_slots = 0
    bit_slots = 0
    units = set()
    kinds = set()
    for model in config[CONF_MODELS]:
        cg.add(var.add_wanted_model(model))
        # Only the tables of the wanted models are compiled in.
        cg.add_define(f"SUNSPEC_MODEL_{model}")
        info = MODEL_INFO[model]
        slots += info["points"] + info["group_points"] * config[CONF_MAX_MODULES]
        text_slots += (
            info["text_points"] + info["text_group_points"] * config[CONF_MAX_MODULES]
        )
        units.update(info["units"])
        kinds.update(info["kinds"])

    if config[CONF_BITFIELD_BINARY_SENSORS]:
        for model in config[CONF_MODELS]:
            info = MODEL_INFO[model]
            bit_slots += (
                info["bits_points"] + info["bits_group_points"] * config[CONF_MAX_MODULES]
            )
    # Reserve the sensor slots (ESPHOME_ENTITY_SENSOR_COUNT is generated from CORE.platform_counts at
    # CoroPriority.FINAL, i.e. after this to_code()).
    CORE.platform_counts["sensor"] += slots
    CORE.platform_counts["text_sensor"] += text_slots
    CORE.platform_counts["binary_sensor"] += bit_slots

    # Unit and device class strings must live in the generated tables; C++ only gets their indices.
    for unit in sorted(units):
        cg.add(var.add_unit_index(unit, register_unit_of_measurement(unit)))
    for kind in sorted(kinds):
        if kind in DEVICE_CLASSES:
            cg.add(
                var.set_device_class_index(
                    POINT_KINDS.index(kind),
                    register_device_class(DEVICE_CLASSES[kind]),
                )
            )

    # Entity icons (stored as an index in the packed entity fields, like the units).
    cg.add_define("USE_ENTITY_ICON")
    for kind, icon in ICONS.items():
        cg.add(var.set_icon_index(POINT_KINDS.index(kind), register_icon(icon)))
    n = len(POINT_KINDS)
    cg.add(var.set_icon_index(n, register_icon(ICON_PERCENT)))
    cg.add(var.set_icon_index(n + 1, register_icon(ICON_INFO)))
    cg.add(var.set_icon_index(n + 2, register_icon(ICON_ALERT)))
