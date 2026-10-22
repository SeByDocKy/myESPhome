# Updated : 2026.01.13
# Version : 1.1.5
# GitHub  : https://github.com/Sleeper85/esphome-components

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_DEVICE_CLASS,
    ENTITY_CATEGORY_CONFIG,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_BATTERY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    ICON_EMPTY,
    ICON_COUNTER,
    ICON_TIMELAPSE,
    UNIT_EMPTY,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

from .. import CONF_JK_RS485_BMS_ID, JK_RS485_BMS_COMPONENT_SCHEMA, jk_rs485_bms_ns

DEPENDENCIES = ["jk_rs485_bms"]
CODEOWNERS = ["@syssi","@txubelaxu","@Sleeper85"]

DEFAULT_STEP = 1

UNIT_MICROSECONDS = "us"
UNIT_SECONDS = "s"
UNIT_HOURS = "h"
UNIT_OHM = "Ω"
UNIT_AMPERE_HOURS = "Ah"

CONF_CELL_SMART_SLEEP_VOLTAGE = "cell_smart_sleep_voltage"
CONF_CELL_UNDERVOLTAGE_PROTECTION = "cell_undervoltage_protection"
CONF_CELL_UNDERVOLTAGE_PROTECTION_RECOVERY = "cell_undervoltage_protection_recovery"
CONF_CELL_OVERVOLTAGE_PROTECTION = "cell_overvoltage_protection"
CONF_CELL_OVERVOLTAGE_PROTECTION_RECOVERY = "cell_overvoltage_protection_recovery"
CONF_CELL_BALANCING_TRIGGER_VOLTAGE = "cell_balancing_trigger_voltage"
CONF_CELL_SOC100_VOLTAGE = "cell_soc100_voltage"
CONF_CELL_SOC0_VOLTAGE = "cell_soc0_voltage"
CONF_CELL_REQUEST_CHARGE_VOLTAGE = "cell_request_charge_voltage"
CONF_CELL_REQUEST_FLOAT_VOLTAGE = "cell_request_float_voltage"
CONF_CELL_POWER_OFF_VOLTAGE = "cell_power_off_voltage"
CONF_CELL_BALANCING_STARTING_VOLTAGE = "cell_balancing_starting_voltage"

CONF_MAX_CHARGING_CURRENT ="max_charging_current"
CONF_CHARGING_OVERCURRENT_PROTECTION_DELAY = "charging_overcurrent_protection_delay"
CONF_CHARGING_OVERCURRENT_PROTECTION_RECOVERY_DELAY = "charging_overcurrent_protection_recovery_delay"
CONF_MAX_DISCHARGING_CURRENT ="max_discharging_current"
CONF_DISCHARGING_OVERCURRENT_PROTECTION_DELAY = "discharging_overcurrent_protection_delay"
CONF_DISCHARGING_OVERCURRENT_PROTECTION_RECOVERY_DELAY = "discharging_overcurrent_protection_recovery_delay"
CONF_SHORT_CIRCUIT_PROTECTION_DELAY ="short_circuit_protection_delay"
CONF_SHORT_CIRCUIT_PROTECTION_RECOVERY_DELAY = "short_circuit_protection_recovery_delay"
CONF_MAX_BALANCING_CURRENT = "max_balancing_current"

CONF_CHARGING_OVERTEMPERATURE_PROTECTION = "charging_overtemperature_protection"
CONF_CHARGING_OVERTEMPERATURE_PROTECTION_RECOVERY = "charging_overtemperature_protection_recovery"
CONF_DISCHARGING_OVERTEMPERATURE_PROTECTION = "discharging_overtemperature_protection"
CONF_DISCHARGING_OVERTEMPERATURE_PROTECTION_RECOVERY = "discharging_overtemperature_protection_recovery"
CONF_CHARGING_LOWTEMPERATURE_PROTECTION = "charging_lowtemperature_protection"
CONF_CHARGING_LOWTEMPERATURE_PROTECTION_RECOVERY = "charging_lowtemperature_protection_recovery"
CONF_MOS_OVERTEMPERATURE_PROTECTION = "mos_overtemperature_protection"
CONF_MOS_OVERTEMPERATURE_PROTECTION_RECOVERY = "mos_overtemperature_protection_recovery"

CONF_CELL_COUNT_SETTINGS = "cell_count_settings"
CONF_BATTERY_CAPACITY_TOTAL_SETTINGS = "battery_capacity_total_settings"
CONF_PRECHARGING_TIME_FROM_DISCHARGE = "precharging_time_from_discharge";
CONF_CELL_REQUEST_CHARGE_VOLTAGE_TIME = "cell_request_charge_voltage_time"
CONF_CELL_REQUEST_FLOAT_VOLTAGE_TIME = "cell_request_float_voltage_time"
CONF_VOLTAGE_CALIBRATION = "voltage_calibration"
CONF_CURRENT_CALIBRATION = "current_calibration"

ICON_BALANCING = "mdi:seesaw"
ICON_CURRENT_DC = "mdi:current-dc"
ICON_CELL_VOLTAGE_MIN_CELL_NUMBER = "mdi:battery-minus-outline"
ICON_CELL_VOLTAGE_MAX_CELL_NUMBER = "mdi:battery-plus-outline"

ICON_BATTERY_STRINGS = "mdi:car-battery"
ICON_BATTERY_CAPACITY_REMAINING = "mdi:battery-50"
ICON_BATTERY_CAPACITY_REMAINING_DERIVED = "mdi:battery-50"
ICON_ACTUAL_BATTERY_CAPACITY = "mdi:battery-50"
ICON_BATTERY_CAPACITY_TOTAL_SETTING = "mdi:battery-sync"

ICON_DEVICE_ADDRESS = "mdi:identifier"
ICON_ERRORS_BITMASK = "mdi:alert-circle-outline"
ICON_OPERATION_MODE_BITMASK = "mdi:heart-pulse"
ICON_CHARGING_CYCLES = "mdi:battery-sync"
ICON_ALARM_LOW_VOLUME = "mdi:volume-high"

ICON_CELL_RESISTANCE = "mdi:omega"
ICON_BALANCING = "mdi:seesaw"
ICON_DIRECTION = "mdi:swap-horizontal-bold"
ICON_CLOCK ="mdi:clock-outline"

ICON_HIGH_TEMPERATURE = "mdi:weather-sunny"
ICON_LOW_TEMPERATURE = "mdi:snowflake"

UNIT_AMPERE_HOUR = "Ah"

NUMBERS = {
    # JK04, JK02_24S, JK02_32S, factor, length
    CONF_CELL_SMART_SLEEP_VOLTAGE:                          [0x0000, 0x10, 0x04,3,0],
    CONF_CELL_UNDERVOLTAGE_PROTECTION:                      [0x0004, 0x10, 0x04,3,0],
    CONF_CELL_UNDERVOLTAGE_PROTECTION_RECOVERY:             [0x0008, 0x10, 0x04,3,0],
    CONF_CELL_OVERVOLTAGE_PROTECTION:                       [0x000C, 0x10, 0x04,3,0],
    CONF_CELL_OVERVOLTAGE_PROTECTION_RECOVERY:              [0x0010, 0x10, 0x04,3,0],
    CONF_CELL_BALANCING_TRIGGER_VOLTAGE:                    [0x0014, 0x10, 0x04,3,0],
    CONF_CELL_SOC100_VOLTAGE:                               [0x0018, 0x10, 0x04,3,0],
    CONF_CELL_SOC0_VOLTAGE:                                 [0x001C, 0x10, 0x04,3,0],
    CONF_CELL_REQUEST_CHARGE_VOLTAGE:                       [0x0020, 0x10, 0x04,3,0],
    CONF_CELL_REQUEST_FLOAT_VOLTAGE:                        [0x0024, 0x10, 0x04,3,0],
    CONF_CELL_POWER_OFF_VOLTAGE:                            [0x0028, 0x10, 0x04,3,0],
    CONF_CELL_BALANCING_STARTING_VOLTAGE:                   [0x0084, 0x10, 0x04,3,0],
    CONF_MAX_CHARGING_CURRENT:                              [0x002C, 0x10, 0x04,3,0], 
    CONF_CHARGING_OVERCURRENT_PROTECTION_DELAY:             [0x0030, 0x10, 0x04,0,0],
    CONF_CHARGING_OVERCURRENT_PROTECTION_RECOVERY_DELAY:    [0x0034, 0x10, 0x04,0,0],
    CONF_MAX_DISCHARGING_CURRENT:                           [0x0038, 0x10, 0x04,3,0],
    CONF_DISCHARGING_OVERCURRENT_PROTECTION_DELAY:          [0x003C, 0x10, 0x04,0,0],
    CONF_DISCHARGING_OVERCURRENT_PROTECTION_RECOVERY_DELAY: [0x0040, 0x10, 0x04,0,0],
    CONF_SHORT_CIRCUIT_PROTECTION_DELAY:                    [0x0080, 0x10, 0x04,6,0],
    CONF_SHORT_CIRCUIT_PROTECTION_RECOVERY_DELAY:           [0x0044, 0x10, 0x04,0,0],
    CONF_MAX_BALANCING_CURRENT:                             [0x0048, 0x10, 0x04,3,0],
    CONF_CHARGING_OVERTEMPERATURE_PROTECTION:               [0x004C, 0x10, 0x04,1,1],
    CONF_CHARGING_OVERTEMPERATURE_PROTECTION_RECOVERY:      [0x0050, 0x10, 0x04,1,1],
    CONF_DISCHARGING_OVERTEMPERATURE_PROTECTION:            [0x0054, 0x10, 0x04,1,1],
    CONF_DISCHARGING_OVERTEMPERATURE_PROTECTION_RECOVERY:   [0x0058, 0x10, 0x04,1,1],
    CONF_CHARGING_LOWTEMPERATURE_PROTECTION:                [0x005C, 0x10, 0x04,1,1],
    CONF_CHARGING_LOWTEMPERATURE_PROTECTION_RECOVERY:       [0x0060, 0x10, 0x04,1,1],
    CONF_MOS_OVERTEMPERATURE_PROTECTION:                    [0x0064, 0x10, 0x04,1,1],
    CONF_MOS_OVERTEMPERATURE_PROTECTION_RECOVERY:           [0x0068, 0x10, 0x04,1,1],
    CONF_CELL_COUNT_SETTINGS:                               [0x006C, 0x10, 0x04,0,0],
    CONF_BATTERY_CAPACITY_TOTAL_SETTINGS:                   [0x007C, 0x10, 0x04,3,0],
    CONF_PRECHARGING_TIME_FROM_DISCHARGE:                   [0x010C, 0x10, 0x04,3,0], # TO VERIFY !
    CONF_CELL_REQUEST_CHARGE_VOLTAGE_TIME:                  [0x0104, 0x15, 0x02,1,0],
    CONF_CELL_REQUEST_FLOAT_VOLTAGE_TIME:                   [0x0104, 0x15, 0x02,1,0],
}

JkRS485BmsNumber = jk_rs485_bms_ns.class_("JkRS485BmsNumber", number.Number, cg.Component)

JK_RS485_NUMBER_SCHEMA = (
    number.number_schema(
        JkRS485BmsNumber,
        icon=ICON_EMPTY,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_VOLT,
    )
    .extend(
        {
            cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
            # cv.Optional(CONF_STEP, default=0.01): cv.float_,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = JK_RS485_BMS_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_CELL_SMART_SLEEP_VOLTAGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ),
        cv.Optional(CONF_CELL_UNDERVOLTAGE_PROTECTION): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ),
        cv.Optional(CONF_CELL_UNDERVOLTAGE_PROTECTION_RECOVERY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ),
        cv.Optional(CONF_CELL_OVERVOLTAGE_PROTECTION): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ),
        cv.Optional(CONF_CELL_OVERVOLTAGE_PROTECTION_RECOVERY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ),
        cv.Optional(CONF_CELL_BALANCING_TRIGGER_VOLTAGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0.001): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=1.0): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ), 
        cv.Optional(CONF_CELL_SOC100_VOLTAGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ), 
        cv.Optional(CONF_CELL_SOC0_VOLTAGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ), 
        cv.Optional(CONF_CELL_REQUEST_CHARGE_VOLTAGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ), 
        cv.Optional(CONF_CELL_REQUEST_FLOAT_VOLTAGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ),
        cv.Optional(CONF_CELL_POWER_OFF_VOLTAGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ),
        cv.Optional(CONF_CELL_BALANCING_STARTING_VOLTAGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=1.2): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=4.350): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
            }
        ),
        cv.Optional(CONF_MAX_CHARGING_CURRENT): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_AMPERE): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CLOCK): cv.icon, 
            }
        ),
        cv.Optional(CONF_CHARGING_OVERCURRENT_PROTECTION_DELAY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=3600): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_SECONDS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CLOCK): cv.icon, 
            }
        ),
        cv.Optional(CONF_CHARGING_OVERCURRENT_PROTECTION_RECOVERY_DELAY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=3600): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_SECONDS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CLOCK): cv.icon, 
            }
        ), 
        cv.Optional(CONF_MAX_DISCHARGING_CURRENT): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_AMPERE): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CURRENT_DC): cv.icon, 
            }
        ),
        cv.Optional(CONF_DISCHARGING_OVERCURRENT_PROTECTION_DELAY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=3600): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_SECONDS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CLOCK): cv.icon, 
            }
        ),
        cv.Optional(CONF_DISCHARGING_OVERCURRENT_PROTECTION_RECOVERY_DELAY): JK_RS485_NUMBER_SCHEMA.extend(
            { 
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=3600): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_SECONDS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CLOCK): cv.icon,
            }
        ), 

        cv.Optional(CONF_SHORT_CIRCUIT_PROTECTION_DELAY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=3600): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_MICROSECONDS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CLOCK): cv.icon, 
            }
        ), 
        cv.Optional(CONF_SHORT_CIRCUIT_PROTECTION_RECOVERY_DELAY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=3600): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_SECONDS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CLOCK): cv.icon, 
            }
        ), 
        cv.Optional(CONF_MAX_BALANCING_CURRENT): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=2): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_AMPERE): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CURRENT_DC): cv.icon, 
            }
        ),
        cv.Optional(CONF_CHARGING_OVERTEMPERATURE_PROTECTION): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_HIGH_TEMPERATURE): cv.icon, 
            }
        ),
        cv.Optional(CONF_CHARGING_OVERTEMPERATURE_PROTECTION_RECOVERY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_HIGH_TEMPERATURE): cv.icon, 
            }
        ),
        cv.Optional(CONF_DISCHARGING_OVERTEMPERATURE_PROTECTION): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_HIGH_TEMPERATURE): cv.icon, 
            }
        ),
        cv.Optional(CONF_DISCHARGING_OVERTEMPERATURE_PROTECTION_RECOVERY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_HIGH_TEMPERATURE): cv.icon, 
            }
        ),
        cv.Optional(CONF_CHARGING_LOWTEMPERATURE_PROTECTION): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=-100): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_LOW_TEMPERATURE): cv.icon, 
            }
        ),
        cv.Optional(CONF_CHARGING_LOWTEMPERATURE_PROTECTION_RECOVERY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=-100): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_LOW_TEMPERATURE): cv.icon, 
            }
        ),
        cv.Optional(CONF_MOS_OVERTEMPERATURE_PROTECTION): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_HIGH_TEMPERATURE): cv.icon, 
            }
        ),
        cv.Optional(CONF_MOS_OVERTEMPERATURE_PROTECTION_RECOVERY): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=200): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_HIGH_TEMPERATURE): cv.icon, 
            }
        ),
        cv.Optional(CONF_CELL_COUNT_SETTINGS): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=3): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=24): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_EMPTY): cv.string_strict,
            }
        ),
        cv.Optional(CONF_BATTERY_CAPACITY_TOTAL_SETTINGS): JK_RS485_NUMBER_SCHEMA.extend(
            { 
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=3000): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_AMPERE_HOURS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_BATTERY_CAPACITY_TOTAL_SETTING): cv.icon,
            }
        ), 
        cv.Optional(CONF_PRECHARGING_TIME_FROM_DISCHARGE): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=3600): cv.float_,
                cv.Optional(CONF_STEP, default=1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_SECONDS): cv.string_strict,
                cv.Optional(CONF_ICON, default=ICON_CLOCK): cv.icon, 
            }
        ), 
        cv.Optional(CONF_CELL_REQUEST_CHARGE_VOLTAGE_TIME): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=255): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_HOURS): cv.string_strict,
            }
        ), 
        cv.Optional(CONF_CELL_REQUEST_FLOAT_VOLTAGE_TIME): JK_RS485_NUMBER_SCHEMA.extend(
            {
                cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=255): cv.float_,
                cv.Optional(CONF_STEP, default=0.1): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_HOURS): cv.string_strict,
            }
        ),
        cv.Optional(CONF_VOLTAGE_CALIBRATION): JK_RS485_NUMBER_SCHEMA.extend(
            {
                # @FIXME The exact limits are unknown
                cv.Optional(CONF_MIN_VALUE, default=10.0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=100.0): cv.float_,
                cv.Optional(CONF_STEP, default=0.01): cv.float_,
            }
        ),
        cv.Optional(CONF_CURRENT_CALIBRATION): JK_RS485_NUMBER_SCHEMA.extend(
            {
                # @FIXME Exact limits are unknown
                cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
                cv.Optional(CONF_MAX_VALUE, default=100.0): cv.float_,
                cv.Optional(CONF_STEP, default=0.001): cv.float_,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_AMPERE): cv.string_strict,
            }
        ),
    }
)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_JK_RS485_BMS_ID])
    for key, param_config in NUMBERS.items():
        if key in config:
            conf = config[key]
            var = cg.new_Pvariable(conf[CONF_ID])
            await cg.register_component(var, conf)
            await number.register_number(
                var,
                conf,
                min_value=conf[CONF_MIN_VALUE],
                max_value=conf[CONF_MAX_VALUE],
                step=conf[CONF_STEP],
            )
            cg.add(getattr(hub, f"set_{key}_number")(var))
            cg.add(var.set_parent(hub))
            cg.add(var.set_register_address(param_config[0]))
            cg.add(var.set_third_element_of_frame(param_config[1]))
            cg.add(var.set_data_length(param_config[2]))
            cg.add(var.set_factor(param_config[3]))
            cg.add(var.set_type(param_config[4]))
