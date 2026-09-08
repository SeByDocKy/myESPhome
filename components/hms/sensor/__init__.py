import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import sensor
from esphome.const import (
    CONF_CURRENT,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_POWER,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_EMPTY,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

from .. import HMSComponent, CONF_SN

CONF_HMS_ID = "hms_id"
CONF_DC_CHANNELS = "dc_channels"
CONF_AC = "ac"
CONF_INVERTER = "inverter"
CONF_ENERGY_TODAY = "energy_today"
CONF_ENERGY_TOTAL = "energy_total"
CONF_POWER_FACTOR = "power_factor"
CONF_REACTIVE_POWER = "reactive_power"
CONF_EFFICIENCY = "efficiency"
CONF_RSSI = "rssi"

DEPENDENCIES = ["hms"]

_POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:power",
)
_DC_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="A",
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:current-dc",
)
_AC_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="A",
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:current-ac",
)
_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:sine-wave",
)
_ENERGY_TODAY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT_HOURS,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    accuracy_decimals=0,
    icon="mdi:counter",
)
_ENERGY_TOTAL_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOWATT_HOURS,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    accuracy_decimals=3,
    icon="mdi:counter",
)
_FREQUENCY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
    icon="mdi:metronome",
)
_POWER_FACTOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_EMPTY,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=3,
    icon="mdi:angle-acute",
)
_REACTIVE_POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:power",
)
_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:thermometer",
)
_EFFICIENCY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=3,
    icon="mdi:percent",
)
_RSSI_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    accuracy_decimals=0,
    icon="mdi:signal",
)

DC_CHANNEL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_CURRENT): _DC_CURRENT_SCHEMA,
        cv.Optional(CONF_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_ENERGY_TODAY): _ENERGY_TODAY_SCHEMA,
        cv.Optional(CONF_ENERGY_TOTAL): _ENERGY_TOTAL_SCHEMA,
    }
)

AC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): _VOLTAGE_SCHEMA,
        cv.Optional(CONF_CURRENT): _AC_CURRENT_SCHEMA,
        cv.Optional(CONF_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_FREQUENCY): _FREQUENCY_SCHEMA,
        cv.Optional(CONF_POWER_FACTOR): _POWER_FACTOR_SCHEMA,
        cv.Optional(CONF_REACTIVE_POWER): _REACTIVE_POWER_SCHEMA,
    }
)

INVERTER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TEMPERATURE): _TEMPERATURE_SCHEMA,
        cv.Optional(CONF_POWER): _POWER_SCHEMA,
        cv.Optional(CONF_ENERGY_TODAY): _ENERGY_TODAY_SCHEMA,
        cv.Optional(CONF_ENERGY_TOTAL): _ENERGY_TOTAL_SCHEMA,
        cv.Optional(CONF_EFFICIENCY): _EFFICIENCY_SCHEMA,
    }
)

def _dc_channel_entry(value):
    value = cv.Schema({cv.string: DC_CHANNEL_SCHEMA})(value)
    if len(value) != 1:
        raise cv.Invalid(
            "Chaque entrée de 'dc_channels' doit contenir exactement un nom de canal "
            "(ex. 'pv0: {power: {name: ...}}')."
        )
    return value


def _validate_dc_channels_list(value):
    value = cv.ensure_list(_dc_channel_entry)(value)
    cv.Length(min=1, max=4)(value)

    seen = set()
    for entry in value:
        label = next(iter(entry))
        if label in seen:
            raise cv.Invalid(
                f"Le nom de canal '{label}' est utilisé plusieurs fois dans "
                f"'dc_channels' -- chaque canal doit avoir un nom unique (pv0, pv1, ...)."
            )
        seen.add(label)
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_ID): cv.use_id(HMSComponent),
        cv.Optional(CONF_DC_CHANNELS): _validate_dc_channels_list,
        cv.Optional(CONF_AC): AC_SCHEMA,
        cv.Optional(CONF_INVERTER): INVERTER_SCHEMA,
        cv.Optional(CONF_RSSI): _RSSI_SCHEMA,
    }
)


# Duplique la table de décodage préfixe SN -> nombre de canaux DC de hms.cpp
# (decode_serial_ / inverters/HMS_1CH.cpp, HMS_2CH.cpp, HMS_4CH.cpp) pour pouvoir
# détecter un mismatch dès la validation YAML, avant même la compilation C++.
def _hms_prefix_channel_count(sn: str):
    prefix = int(sn[:4], 16)
    if prefix == 0x1124:
        return 1
    if prefix in (0x1143, 0x1144, 0x1410, 0x114A):
        return 2
    if prefix in (0x1164, 0x1166, 0x1420):
        return 4
    return None  # préfixe inconnu -- laissé à l'erreur runtime de decode_serial_()


def _final_validate(config):
    try:
        full_conf = fv.full_config.get()
        hms_confs = full_conf.get("hms", [])
        if isinstance(hms_confs, dict):
            hms_confs = [hms_confs]
    except Exception:  # noqa: BLE001 -- API interne fv.full_config potentiellement
        # différente selon la version d'ESPHome ; on ne bloque pas la compilation
        # pour autant, on saute juste la validation croisée dans ce cas.
        return config

    hms_id = config[CONF_HMS_ID]
    for hms_conf in hms_confs:
        if hms_conf.get(CONF_ID) != hms_id:
            continue

        sn = hms_conf.get(CONF_SN)
        if sn is None:
            break

        expected = _hms_prefix_channel_count(sn)
        if expected is None:
            break  # préfixe inconnu -- le hub lèvera sa propre erreur au boot

        declared = len(config.get(CONF_DC_CHANNELS, []))
        if declared > expected:
            raise cv.Invalid(
                f"Le numéro de série '{sn}' correspond à un onduleur à {expected} "
                f"canal/canaux DC, mais {declared} entrée(s) 'dc_channels' sont "
                f"déclarées ici -- les canaux excédentaires ne recevront jamais de "
                f"valeur. Réduis 'dc_channels' à {expected} entrée(s)."
            )
        break

    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_ID])

    for i, entry in enumerate(config.get(CONF_DC_CHANNELS, [])):
        _label, channel = next(iter(entry.items()))
        if CONF_POWER in channel:
            s = await sensor.new_sensor(channel[CONF_POWER])
            cg.add(hub.set_dc_power_sensor(i, s))
        if CONF_CURRENT in channel:
            s = await sensor.new_sensor(channel[CONF_CURRENT])
            cg.add(hub.set_dc_current_sensor(i, s))
        if CONF_VOLTAGE in channel:
            s = await sensor.new_sensor(channel[CONF_VOLTAGE])
            cg.add(hub.set_dc_voltage_sensor(i, s))
        if CONF_ENERGY_TODAY in channel:
            s = await sensor.new_sensor(channel[CONF_ENERGY_TODAY])
            cg.add(hub.set_dc_energy_today_sensor(i, s))
        if CONF_ENERGY_TOTAL in channel:
            s = await sensor.new_sensor(channel[CONF_ENERGY_TOTAL])
            cg.add(hub.set_dc_energy_total_sensor(i, s))

    if CONF_AC in config:
        ac = config[CONF_AC]
        if CONF_VOLTAGE in ac:
            s = await sensor.new_sensor(ac[CONF_VOLTAGE])
            cg.add(hub.set_ac_voltage_sensor(s))
        if CONF_CURRENT in ac:
            s = await sensor.new_sensor(ac[CONF_CURRENT])
            cg.add(hub.set_ac_current_sensor(s))
        if CONF_POWER in ac:
            s = await sensor.new_sensor(ac[CONF_POWER])
            cg.add(hub.set_ac_power_sensor(s))
        if CONF_FREQUENCY in ac:
            s = await sensor.new_sensor(ac[CONF_FREQUENCY])
            cg.add(hub.set_ac_frequency_sensor(s))
        if CONF_POWER_FACTOR in ac:
            s = await sensor.new_sensor(ac[CONF_POWER_FACTOR])
            cg.add(hub.set_ac_power_factor_sensor(s))
        if CONF_REACTIVE_POWER in ac:
            s = await sensor.new_sensor(ac[CONF_REACTIVE_POWER])
            cg.add(hub.set_ac_reactive_power_sensor(s))

    if CONF_INVERTER in config:
        inv = config[CONF_INVERTER]
        if CONF_TEMPERATURE in inv:
            s = await sensor.new_sensor(inv[CONF_TEMPERATURE])
            cg.add(hub.set_inv_temperature_sensor(s))
        if CONF_POWER in inv:
            s = await sensor.new_sensor(inv[CONF_POWER])
            cg.add(hub.set_inv_power_sensor(s))
        if CONF_ENERGY_TODAY in inv:
            s = await sensor.new_sensor(inv[CONF_ENERGY_TODAY])
            cg.add(hub.set_inv_energy_today_sensor(s))
        if CONF_ENERGY_TOTAL in inv:
            s = await sensor.new_sensor(inv[CONF_ENERGY_TOTAL])
            cg.add(hub.set_inv_energy_total_sensor(s))
        if CONF_EFFICIENCY in inv:
            s = await sensor.new_sensor(inv[CONF_EFFICIENCY])
            cg.add(hub.set_inv_efficiency_sensor(s))

    if CONF_RSSI in config:
        s = await sensor.new_sensor(config[CONF_RSSI])
        cg.add(hub.set_rssi_sensor(s))
