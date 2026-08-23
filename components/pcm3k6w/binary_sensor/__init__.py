import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_PROBLEM

from .. import CONF_PCM3K6W_ID, PCM3K6W_PLATFORM_SCHEMA, pcm3k6w_ns

DEPENDENCIES = ["canbus"]

PCM3K6WBinarySensor = pcm3k6w_ns.class_("PCM3K6WBinarySensor", binary_sensor.BinarySensor)

# Kind indices MUST match the BinarySensorKind enum order in pcm3k6w.h.
(
    KIND_BATTERY_UNDERVOLTAGE_ALARM,
    KIND_BATTERY_OVERVOLTAGE_ALARM,
    KIND_GRID_UNDERVOLTAGE_ALARM,
    KIND_GRID_OVERVOLTAGE_ALARM,
    KIND_FAULT_STATUS,
    KIND_FAULT_SOFT_START_TIMEOUT,
    KIND_FAULT_BUS_OVERVOLTAGE,
    KIND_FAULT_BUS_UNDERVOLTAGE,
    KIND_FAULT_INVERTER_OVERVOLTAGE,
    KIND_FAULT_INVERTER_UNDERVOLTAGE,
    KIND_FAULT_INVERTER_RMS_OVERCURRENT,
    KIND_FAULT_INVERTER_FAST_OVERCURRENT,
    KIND_FAULT_DC_OVERVOLTAGE,
    KIND_FAULT_DC_UNDERVOLTAGE,
    KIND_FAULT_DC_FAST_OVERCURRENT,
    KIND_FAULT_DC_RMS_OVERCURRENT,
    KIND_FAULT_LOAD_OVERCURRENT,
    KIND_FAULT_SOFT_START_SHORT_CIRCUIT,
    KIND_FAULT_OUTPUT_OVERLOAD,
    KIND_FAULT_OVER_TEMPERATURE,
    KIND_FAULT_FAN_FAULT,
    KIND_FAULT_SYNC_SIGNAL_LOSS,
    KIND_FAULT_LOCKED,
    KIND_FAULT_ADDRESS_FAULT,
    KIND_FAULT_SN_REPETITION,
    KIND_FAULT_LOAD_RMS_OVERCURRENT,
    KIND_FAULT_OVERLOAD,
) = range(27)

# (config key, kind, icon)
BINARY_SENSORS = [
    ("battery_undervoltage_alarm", KIND_BATTERY_UNDERVOLTAGE_ALARM, "mdi:battery-alert-variant-outline"),
    ("battery_overvoltage_alarm", KIND_BATTERY_OVERVOLTAGE_ALARM, "mdi:battery-alert"),
    ("grid_undervoltage_alarm", KIND_GRID_UNDERVOLTAGE_ALARM, "mdi:flash-alert-outline"),
    ("grid_overvoltage_alarm", KIND_GRID_OVERVOLTAGE_ALARM, "mdi:flash-alert"),
    ("fault_status", KIND_FAULT_STATUS, "mdi:alert-circle"),
    ("fault_soft_start_timeout", KIND_FAULT_SOFT_START_TIMEOUT, "mdi:clock-alert"),
    ("fault_bus_overvoltage", KIND_FAULT_BUS_OVERVOLTAGE, "mdi:flash-alert"),
    ("fault_bus_undervoltage", KIND_FAULT_BUS_UNDERVOLTAGE, "mdi:flash-alert-outline"),
    ("fault_inverter_overvoltage", KIND_FAULT_INVERTER_OVERVOLTAGE, "mdi:flash-alert"),
    ("fault_inverter_undervoltage", KIND_FAULT_INVERTER_UNDERVOLTAGE, "mdi:flash-alert"),
    ("fault_inverter_rms_overcurrent", KIND_FAULT_INVERTER_RMS_OVERCURRENT, "mdi:flash-alert"),
    ("fault_inverter_fast_overcurrent", KIND_FAULT_INVERTER_FAST_OVERCURRENT, "mdi:flash-alert"),
    ("fault_dc_overvoltage", KIND_FAULT_DC_OVERVOLTAGE, "mdi:battery-alert"),
    ("fault_dc_undervoltage", KIND_FAULT_DC_UNDERVOLTAGE, "mdi:battery-alert-variant-outline"),
    ("fault_dc_fast_overcurrent", KIND_FAULT_DC_FAST_OVERCURRENT, "mdi:battery-alert"),
    ("fault_dc_rms_overcurrent", KIND_FAULT_DC_RMS_OVERCURRENT, "mdi:battery-alert"),
    ("fault_load_overcurrent", KIND_FAULT_LOAD_OVERCURRENT, None),
    ("fault_soft_start_short_circuit", KIND_FAULT_SOFT_START_SHORT_CIRCUIT, None),
    ("fault_output_overload", KIND_FAULT_OUTPUT_OVERLOAD, None),
    ("fault_over_temperature", KIND_FAULT_OVER_TEMPERATURE, "mdi:car-brake-temperature"),
    ("fault_fan_fault", KIND_FAULT_FAN_FAULT, "mdi:fan-alert"),
    ("fault_sync_signal_loss", KIND_FAULT_SYNC_SIGNAL_LOSS, None),
    ("fault_locked", KIND_FAULT_LOCKED, None),
    ("fault_address_fault", KIND_FAULT_ADDRESS_FAULT, None),
    ("fault_sn_repetition", KIND_FAULT_SN_REPETITION, None),
    ("fault_load_rms_overcurrent", KIND_FAULT_LOAD_RMS_OVERCURRENT, None),
    ("fault_overload", KIND_FAULT_OVERLOAD, None),
]

CONFIG_SCHEMA = PCM3K6W_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(key): binary_sensor.binary_sensor_schema(
            PCM3K6WBinarySensor,
            device_class=DEVICE_CLASS_PROBLEM,
            **({"icon": icon} if icon else {}),
        )
        for key, _kind, icon in BINARY_SENSORS
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PCM3K6W_ID])
    for key, kind, _icon in BINARY_SENSORS:
        if key in config:
            s = await binary_sensor.new_binary_sensor(config[key])
            cg.add(hub.set_binary_sensor(kind, s))
