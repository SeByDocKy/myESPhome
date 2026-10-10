"""Register table of the Deye single-phase hybrid inverter (SUN-xK-SG0x-LP1 family).

Generated once from the two original modbus_controller YAML files (nominal + advanced) and meant to be edited by hand.
Every entity of the component is one entry of one of the tables below; the key is the name of the entity in the YAML
of the platform (`sensor: - platform: deye_mono  <key>: {name: ...}`).

Common fields
  register   holding register address
  vtype      ESPHome modbus value type (U_WORD, S_WORD, U_DWORD_R ...)
  bridge     the register is joined to the read range before it even across a gap of unused registers
             (RangeReuse.ALWAYS, the former `reuse_previous_range: true`): keeps the number of read frames low
  unit, icon, device_class, state_class, precision, category   usual ESPHome entity attributes

sensor      value = (raw + add) * scale [* inverter_factor] [* -1]
            wrap: raw > 32767 -> raw - 65535 (the time-of-use start times)
            kind "calc": computed from registers it reads itself (`inputs`, same transform as a sensor); op is
            linear (sum of coefs[i] * input[i]), charge_current, discharge_current, charge_power, discharge_power;
            factor / negate apply to the result of the charge_power / discharge_power ops.
binary      state = (register & mask) != 0
switch      same state; a write changes only the bits of `mask` (the other bits keep the value last read)
number      value = raw * scale; written as round(value / scale)
select      option n <-> (register & mask) == value n; a write changes only the bits of `mask`
text        kind "state" (inverter overall state, text of the value) or "time" (HHMM start time as HH:MM)
"""

SENSORS = {
    'modbus_address': {'register': 37, 'vtype': 'U_WORD', 'add': 1, 'bridge': True},
    'limiter': {'register': 55, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'device_class': 'power', 'precision': 2},
    'gen_energy_today': {'register': 62, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 2, 'bridge': True},
    'battery_charging_energy_today': {'register': 70, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1},
    'battery_discharging_energy_today': {'register': 71, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1},
    'battery_charging_total': {'register': 72, 'vtype': 'U_DWORD_R', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1},
    'battery_discharging_total': {'register': 74, 'vtype': 'U_DWORD_R', 'scale': 0.1, 'unit': 'kWh', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 0},
    'grid_energy_imported_today': {'register': 76, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1},
    'grid_energy_exported_today': {'register': 77, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1},
    'grid_energy_imported_total': {'register': 78, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1},
    'grid_frequency': {'register': 79, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'Hz', 'icon': 'mdi:sine-wave', 'state_class': 'measurement', 'precision': 2},
    'grid_energy_exported_total': {'register': 81, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1, 'bridge': True},
    'load_energy_today': {'register': 84, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1, 'bridge': True},
    'load_energy_total': {'register': 85, 'vtype': 'U_DWORD_R', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1},
    'dc_transformer_temperature': {'register': 90, 'vtype': 'S_WORD', 'add': -1000, 'scale': 0.1, 'unit': '°C', 'icon': 'mdi:thermometer', 'device_class': 'temperature', 'state_class': 'measurement', 'precision': 1, 'bridge': True},
    'dc_radiator_temperature': {'register': 91, 'vtype': 'S_WORD', 'add': -1000, 'scale': 0.1, 'unit': '°C', 'icon': 'mdi:thermometer', 'device_class': 'temperature', 'state_class': 'measurement', 'precision': 1},
    'pv_energy_total': {'register': 96, 'vtype': 'U_DWORD_R', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1, 'bridge': True},
    'pv_energy_today': {'register': 108, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'kWh', 'icon': 'mdi:counter', 'device_class': 'energy', 'state_class': 'total_increasing', 'precision': 1},
    'pv1_voltage': {'register': 109, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'pv1_current': {'register': 110, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'A', 'icon': 'mdi:current-dc', 'device_class': 'current', 'state_class': 'measurement', 'precision': 2},
    'pv2_voltage': {'register': 111, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'pv2_current': {'register': 112, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'A', 'icon': 'mdi:current-dc', 'device_class': 'current', 'state_class': 'measurement', 'precision': 2},
    'pv3_voltage': {'register': 113, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'pv3_current': {'register': 114, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'A', 'icon': 'mdi:current-dc', 'device_class': 'current', 'state_class': 'measurement', 'precision': 2},
    'grid_voltage': {'register': 150, 'vtype': 'S_WORD', 'scale': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'ac_output_voltage': {'register': 154, 'vtype': 'U_WORD', 'scale': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1, 'bridge': True},
    'ac_output_current': {'register': 164, 'vtype': 'S_WORD', 'scale': 0.01, 'unit': 'A', 'icon': 'mdi:current-dc', 'device_class': 'current', 'state_class': 'measurement', 'precision': 2, 'bridge': True},
    'aux_output_power': {'register': 166, 'vtype': 'S_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2, 'bridge': True},
    'grid_power_167': {'register': 167, 'vtype': 'S_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2},
    'grid_power': {'register': 169, 'vtype': 'S_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2, 'bridge': True},
    'grid_external_power': {'register': 172, 'vtype': 'S_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2, 'bridge': True},
    'ac_output_power': {'register': 175, 'vtype': 'S_WORD', 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'precision': 2, 'bridge': True},
    'load_power': {'register': 178, 'vtype': 'S_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2, 'bridge': True},
    'battery_temperature': {'register': 182, 'vtype': 'U_WORD', 'add': -1000, 'scale': 0.1, 'unit': '°C', 'icon': 'mdi:thermometer', 'device_class': 'temperature', 'state_class': 'measurement', 'precision': 1, 'bridge': True},
    'battery_voltage': {'register': 183, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'battery_soc': {'register': 184, 'vtype': 'U_WORD', 'unit': '%', 'icon': 'mdi:percent', 'device_class': 'battery', 'precision': 0},
    'pv1_power': {'register': 186, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2, 'bridge': True},
    'pv2_power': {'register': 187, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2},
    'pv3_power': {'register': 188, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2},
    'battery_power': {'register': 190, 'vtype': 'S_WORD', 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'precision': 2, 'bridge': True},
    'battery_current': {'register': 191, 'vtype': 'S_WORD', 'scale': 0.01, 'factor': True, 'negate': True, 'unit': 'A', 'icon': 'mdi:current-dc', 'device_class': 'current', 'state_class': 'measurement', 'precision': 2},
    'load_frequency': {'register': 192, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'Hz', 'icon': 'mdi:sine-wave', 'state_class': 'measurement', 'precision': 2},
    'ac_output_frequency': {'register': 193, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'Hz', 'icon': 'mdi:alpha-Hz-circle-outline', 'state_class': 'measurement', 'precision': 2},
    'grid_connexion': {'register': 194, 'vtype': 'U_WORD', 'state_class': 'measurement', 'precision': 0},
    'battery_equalization_voltage': {'register': 201, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'battery_absorption_voltage': {'register': 202, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'battery_float_voltage': {'register': 203, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'battery_max_charge_current': {'register': 210, 'vtype': 'U_WORD', 'unit': 'A', 'icon': 'mdi:current-dc', 'device_class': 'current', 'precision': 0, 'bridge': True},
    'battery_max_discharge_current': {'register': 211, 'vtype': 'U_WORD', 'unit': 'A', 'icon': 'mdi:current-dc', 'device_class': 'current', 'precision': 0},
    'battery_capacity_shutdown': {'register': 217, 'vtype': 'U_WORD', 'unit': '%', 'icon': 'mdi:percent', 'device_class': 'battery', 'precision': 0, 'bridge': True},
    'battery_shutdown_voltage': {'register': 220, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1, 'bridge': True},
    'battery_restart_voltage': {'register': 221, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'battery_low_voltage': {'register': 222, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'device_class': 'voltage', 'state_class': 'measurement', 'precision': 1},
    'grid_charge_battery_current': {'register': 230, 'vtype': 'U_WORD', 'unit': 'A', 'icon': 'mdi:current-dc', 'device_class': 'current', 'precision': 0, 'bridge': True},
    'grid_peak_shaving_power': {'register': 293, 'vtype': 'U_WORD', 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2, 'bridge': True},
    'firmware_control_board': {'register': 13, 'vtype': 'U_WORD'},
    'firmware_comms_board': {'register': 14, 'vtype': 'U_WORD'},
    'setting_timezone1': {'register': 250, 'vtype': 'U_WORD', 'wrap': True, 'icon': 'mdi:clock', 'bridge': True},
    'setting_timezone2': {'register': 251, 'vtype': 'U_WORD', 'wrap': True, 'icon': 'mdi:clock'},
    'setting_timezone3': {'register': 252, 'vtype': 'U_WORD', 'wrap': True, 'icon': 'mdi:clock'},
    'setting_timezone4': {'register': 253, 'vtype': 'U_WORD', 'icon': 'mdi:clock'},
    'setting_timezone5': {'register': 254, 'vtype': 'U_WORD', 'wrap': True, 'icon': 'mdi:clock'},
    'setting_timezone6': {'register': 255, 'vtype': 'U_WORD', 'wrap': True, 'icon': 'mdi:clock'},
    'power_timezone1': {'register': 256, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'precision': 0},
    'power_timezone2': {'register': 257, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'precision': 0},
    'power_timezone3': {'register': 258, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'precision': 0},
    'power_timezone4': {'register': 259, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'precision': 0},
    'power_timezone5': {'register': 260, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'precision': 0},
    'power_timezone6': {'register': 261, 'vtype': 'U_WORD', 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'precision': 0},
    'battery_voltage_timezone1': {'register': 262, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'precision': 1},
    'battery_voltage_timezone2': {'register': 263, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'precision': 1},
    'battery_voltage_timezone3': {'register': 264, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'precision': 1},
    'battery_voltage_timezone4': {'register': 265, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'precision': 1},
    'battery_voltage_timezone5': {'register': 266, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'precision': 1},
    'battery_voltage_timezone6': {'register': 267, 'vtype': 'U_WORD', 'scale': 0.01, 'unit': 'V', 'icon': 'mdi:sine-wave', 'precision': 1},
    'setting_soc_timezone1': {'register': 268, 'vtype': 'U_WORD', 'unit': '%', 'icon': 'mdi:percent'},
    'setting_soc_timezone2': {'register': 269, 'vtype': 'U_WORD', 'unit': '%', 'icon': 'mdi:percent'},
    'setting_soc_timezone3': {'register': 270, 'vtype': 'U_WORD', 'unit': '%', 'icon': 'mdi:percent'},
    'setting_soc_timezone4': {'register': 271, 'vtype': 'U_WORD', 'unit': '%', 'icon': 'mdi:percent'},
    'setting_soc_timezone5': {'register': 272, 'vtype': 'U_WORD', 'unit': '%', 'icon': 'mdi:percent'},
    'setting_soc_timezone6': {'register': 273, 'vtype': 'U_WORD', 'unit': '%', 'icon': 'mdi:percent'},
    'battery_charging_current': {'kind': 'calc', 'op': 'charge_current', 'inputs': [{'register': 191, 'vtype': 'S_WORD', 'scale': 0.01, 'factor': True, 'negate': True}], 'unit': 'A', 'icon': 'mdi:current-dc', 'precision': 2},
    'battery_discharging_current': {'kind': 'calc', 'op': 'discharge_current', 'inputs': [{'register': 191, 'vtype': 'S_WORD', 'scale': 0.01, 'factor': True, 'negate': True}], 'unit': 'A', 'icon': 'mdi:current-dc', 'precision': 2},
    'battery_charging_power': {'kind': 'calc', 'op': 'charge_power', 'inputs': [{'register': 183, 'vtype': 'U_WORD', 'scale': 0.01}, {'register': 191, 'vtype': 'S_WORD', 'scale': 0.01, 'factor': True, 'negate': True}], 'factor': True, 'unit': 'W', 'icon': 'mdi:power', 'precision': 2},
    'battery_discharging_power': {'kind': 'calc', 'op': 'discharge_power', 'inputs': [{'register': 183, 'vtype': 'U_WORD', 'scale': 0.01}, {'register': 191, 'vtype': 'S_WORD', 'scale': 0.01, 'factor': True, 'negate': True}], 'factor': True, 'negate': True, 'unit': 'W', 'icon': 'mdi:power', 'precision': 2},
    'pv_power_total': {'kind': 'calc', 'op': 'linear', 'inputs': [{'register': 186, 'vtype': 'U_WORD', 'factor': True, 'bridge': True}, {'register': 187, 'vtype': 'U_WORD', 'factor': True}, {'register': 188, 'vtype': 'U_WORD', 'factor': True}], 'coefs': [1, 1, 1], 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2},
    'essential_power': {'kind': 'calc', 'op': 'linear', 'inputs': [{'register': 175, 'vtype': 'S_WORD', 'bridge': True}, {'register': 167, 'vtype': 'S_WORD', 'factor': True}, {'register': 166, 'vtype': 'S_WORD', 'factor': True, 'bridge': True}], 'coefs': [1, 1, -1], 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2},
    'essential_power_1': {'kind': 'calc', 'op': 'linear', 'inputs': [{'register': 175, 'vtype': 'S_WORD', 'bridge': True}, {'register': 169, 'vtype': 'S_WORD', 'factor': True, 'bridge': True}, {'register': 166, 'vtype': 'S_WORD', 'factor': True, 'bridge': True}], 'coefs': [1, 1, -1], 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2},
    'nonessential_power': {'kind': 'calc', 'op': 'linear', 'inputs': [{'register': 172, 'vtype': 'S_WORD', 'factor': True, 'bridge': True}, {'register': 167, 'vtype': 'S_WORD', 'factor': True}], 'coefs': [1, -1], 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2},
    'nonessential_power_1': {'kind': 'calc', 'op': 'linear', 'inputs': [{'register': 172, 'vtype': 'S_WORD', 'factor': True, 'bridge': True}, {'register': 169, 'vtype': 'S_WORD', 'factor': True, 'bridge': True}], 'coefs': [1, -1], 'unit': 'W', 'icon': 'mdi:power', 'device_class': 'power', 'state_class': 'measurement', 'precision': 2},
}

BINARY_SENSORS = {
    'mppt_multipoint_scanning': {'register': 28, 'mask': 32},
    'switch_on_off': {'register': 43, 'mask': 65535, 'bridge': True},
    'island_protection_mode': {'register': 46, 'mask': 65535, 'bridge': True},
    'mppt_number': {'register': 47, 'mask': 65535},
    'GFDI_mode': {'register': 48, 'mask': 65535},
    'grid_connected_status': {'register': 194, 'mask': 65535},
    'setting_grid_charge_timezone1': {'register': 274, 'mask': 1},
    'setting_grid_charge_timezone2': {'register': 275, 'mask': 1},
    'setting_grid_charge_timezone3': {'register': 276, 'mask': 1},
    'setting_grid_charge_timezone4': {'register': 277, 'mask': 1},
    'setting_grid_charge_timezone5': {'register': 278, 'mask': 1},
    'setting_grid_charge_timezone6': {'register': 279, 'mask': 1},
    'setting_gen_charge_timezone1': {'register': 274, 'mask': 2},
    'setting_gen_charge_timezone2': {'register': 275, 'mask': 2},
    'setting_gen_charge_timezone3': {'register': 276, 'mask': 2},
    'setting_gen_charge_timezone4': {'register': 277, 'mask': 2},
    'setting_gen_charge_timezone5': {'register': 278, 'mask': 2},
    'setting_gen_charge_timezone6': {'register': 279, 'mask': 2},
}

SWITCHES = {
    'low_power_mode': {'register': 28, 'mask': 4, 'icon': 'mdi:toggle-switch'},
    'mppt_multipoint_scanning': {'register': 28, 'mask': 32, 'icon': 'mdi:toggle-switch'},
    'low_noise_mode': {'register': 34, 'mask': 1, 'icon': 'mdi:toggle-switch', 'bridge': True},
    'toggle_solar_sell': {'register': 247, 'mask': 1, 'icon': 'mdi:toggle-switch', 'category': 'config', 'bridge': True},
    'toggle_force_generator': {'register': 326, 'mask': 8192, 'icon': 'mdi:toggle-switch'},
    'toggle_system_timer': {'register': 248, 'mask': 1, 'icon': 'mdi:toggle-switch', 'category': 'config'},
}

NUMBERS = {
    'battery_equalization_voltage': {'register': 201, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 50.0, 'max': 61.0, 'step': 0.1, 'mode': 'slider', 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'battery_absorption_voltage': {'register': 202, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 50.0, 'max': 61.0, 'step': 0.1, 'mode': 'slider', 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'battery_float_voltage': {'register': 203, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 50.0, 'max': 61.0, 'step': 0.1, 'mode': 'slider', 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'battery_shuntdown_voltage': {'register': 220, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 40.0, 'max': 52.0, 'step': 0.1, 'mode': 'slider', 'unit': 'V', 'icon': 'mdi:sine-wave', 'bridge': True},
    'battery_low_voltage': {'register': 222, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 40.0, 'max': 52.0, 'step': 0.1, 'mode': 'slider', 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'battery_max_charge_current': {'register': 210, 'vtype': 'U_WORD', 'min': 5, 'max': 185, 'step': 5, 'mode': 'slider', 'unit': 'A', 'icon': 'mdi:current-dc', 'bridge': True},
    'battery_max_discharge_current': {'register': 211, 'vtype': 'U_WORD', 'min': 0, 'max': 185, 'step': 5, 'mode': 'slider', 'unit': 'A', 'icon': 'mdi:current-dc'},
    'grid_charge_battery_current': {'register': 230, 'vtype': 'U_WORD', 'min': 0, 'max': 185, 'step': 5, 'mode': 'slider', 'unit': 'A', 'icon': 'mdi:current-dc', 'bridge': True},
    'max_sell_power': {'register': 245, 'vtype': 'U_WORD', 'min': 0, 'max': 8000, 'step': 500, 'mode': 'slider', 'unit': 'W'},
    'grid_peak_shaving_power': {'register': 293, 'vtype': 'U_WORD', 'min': 0, 'max': 8000, 'step': 500, 'mode': 'slider', 'unit': 'W', 'icon': 'mdi:power', 'bridge': True},
    'set_timezone1': {'register': 250, 'vtype': 'U_WORD', 'min': 0, 'max': 2359, 'step': 1, 'mode': 'box', 'icon': 'mdi:clock-edit', 'bridge': True},
    'set_timezone2': {'register': 251, 'vtype': 'U_WORD', 'min': 0, 'max': 2359, 'step': 1, 'mode': 'box', 'icon': 'mdi:clock-edit'},
    'set_timezone3': {'register': 252, 'vtype': 'U_WORD', 'min': 0, 'max': 2359, 'step': 1, 'mode': 'box', 'icon': 'mdi:clock-edit'},
    'set_timezone4': {'register': 253, 'vtype': 'U_WORD', 'min': 0, 'max': 2359, 'step': 1, 'mode': 'box', 'icon': 'mdi:clock-edit'},
    'set_timezone5': {'register': 254, 'vtype': 'U_WORD', 'min': 0, 'max': 2359, 'step': 1, 'mode': 'box', 'icon': 'mdi:clock-edit'},
    'set_timezone6': {'register': 255, 'vtype': 'U_WORD', 'min': 0, 'max': 2359, 'step': 1, 'mode': 'box', 'icon': 'mdi:clock-edit'},
    'set_power_timezone1': {'register': 256, 'vtype': 'U_WORD', 'min': 0, 'max': 8000, 'step': 100, 'unit': 'W', 'icon': 'mdi:power'},
    'set_power_timezone2': {'register': 257, 'vtype': 'U_WORD', 'min': 0, 'max': 8000, 'step': 100, 'unit': 'W', 'icon': 'mdi:power'},
    'set_power_timezone3': {'register': 258, 'vtype': 'U_WORD', 'min': 0, 'max': 8000, 'step': 100, 'unit': 'W', 'icon': 'mdi:power'},
    'set_power_timezone4': {'register': 259, 'vtype': 'U_WORD', 'min': 0, 'max': 8000, 'step': 100, 'unit': 'W', 'icon': 'mdi:power'},
    'set_power_timezone5': {'register': 260, 'vtype': 'U_WORD', 'min': 0, 'max': 8000, 'step': 100, 'unit': 'W', 'icon': 'mdi:power'},
    'set_power_timezone6': {'register': 261, 'vtype': 'U_WORD', 'min': 0, 'max': 8000, 'step': 100, 'unit': 'W', 'icon': 'mdi:power'},
    'set_battery_voltage_timezone1': {'register': 262, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 45.0, 'max': 63.0, 'step': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'set_battery_voltage_timezone2': {'register': 263, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 45.0, 'max': 63.0, 'step': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'set_battery_voltage_timezone3': {'register': 264, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 45.0, 'max': 63.0, 'step': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'set_battery_voltage_timezone4': {'register': 265, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 45.0, 'max': 63.0, 'step': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'set_battery_voltage_timezone5': {'register': 266, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 45.0, 'max': 63.0, 'step': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'set_battery_voltage_timezone6': {'register': 267, 'vtype': 'U_WORD', 'scale': 0.01, 'min': 45.0, 'max': 63.0, 'step': 0.1, 'unit': 'V', 'icon': 'mdi:sine-wave'},
    'set_soc_timezone1': {'register': 268, 'vtype': 'U_WORD', 'min': 0, 'max': 100, 'step': 5, 'unit': '%', 'icon': 'mdi:percent'},
    'set_soc_timezone2': {'register': 269, 'vtype': 'U_WORD', 'min': 0, 'max': 100, 'step': 5, 'unit': '%', 'icon': 'mdi:percent'},
    'set_soc_timezone3': {'register': 270, 'vtype': 'U_WORD', 'min': 0, 'max': 100, 'step': 5, 'unit': '%', 'icon': 'mdi:percent'},
    'set_soc_timezone4': {'register': 271, 'vtype': 'U_WORD', 'min': 0, 'max': 100, 'step': 5, 'unit': '%', 'icon': 'mdi:percent'},
    'set_soc_timezone5': {'register': 272, 'vtype': 'U_WORD', 'min': 0, 'max': 100, 'step': 5, 'unit': '%', 'icon': 'mdi:percent'},
    'set_soc_timezone6': {'register': 273, 'vtype': 'U_WORD', 'min': 0, 'max': 100, 'step': 5, 'unit': '%', 'icon': 'mdi:percent'},
}

SELECTS = {
    'switch_on_off': {'register': 43, 'mask': 65535, 'options': [['OFF', 0], ['ON', 1]], 'bridge': True},
    'battery_operate': {'register': 213, 'mask': 65535, 'options': [['According to the voltage', 0], ['According to the capacity', 1], ['No battery', 2]], 'bridge': True},
    'energy_pattern': {'register': 243, 'mask': 65535, 'options': [['Battery first', 0], ['Load first', 1]]},
    'work_mode': {'register': 244, 'mask': 65535, 'options': [['Selling First', 0], ['Zero Export + Limit to Load Only', 1], ['Limited to Home', 2]]},
    'grid_peak_shaving': {'register': 280, 'mask': 256, 'options': [['Disabled', 0], ['Enabled', 256]]},
    'tou_jours_semaine': {'register': 248, 'mask': 255, 'options': [['❌ Désactivé', 0], ['✅ Tous les jours', 255], ['Lun – Ven', 63], ['Sam – Dim', 193]], 'icon': 'mdi:calendar-clock', 'category': 'config'},
    'select_charge_timezone1': {'register': 274, 'mask': 15, 'options': [['Grid off, Gen off, GM off, BU off, CH off', 0], ['Grid on, Gen off, GM off, BU off, CH off', 1], ['Grid off, Gen on, GM off, BU off, CH off', 2], ['Grid on, Gen on, GM off, BU off, CH off', 3], ['Grid off, Gen off, GM on, BU off, CH off', 4], ['Grid on, Gen off, GM on, BU off, CH off', 5], ['Grid off, Gen on, GM on, BU off, CH off', 6], ['Grid on, Gen on, GM on, BU off, CH off', 7], ['Grid off, Gen off, GM off, BU on, CH off', 8], ['Grid on, Gen off, GM off, BU on, CH off', 9], ['Grid off, Gen on, GM off, BU on, CH off', 10], ['Grid on, Gen on, GM off, BU on, CH off', 11], ['Grid off, Gen off, GM off, BU off, CH on', 12], ['Grid on, Gen off, GM off, BU off, CH on', 13], ['Grid off, Gen on, GM off, BU off, CH on', 14], ['Grid on, Gen on, GM off, BU off, CH on', 15]], 'icon': 'mdi:numeric', 'category': 'config'},
    'select_charge_timezone2': {'register': 275, 'mask': 15, 'options': [['Grid off, Gen off, GM off, BU off, CH off', 0], ['Grid on, Gen off, GM off, BU off, CH off', 1], ['Grid off, Gen on, GM off, BU off, CH off', 2], ['Grid on, Gen on, GM off, BU off, CH off', 3], ['Grid off, Gen off, GM on, BU off, CH off', 4], ['Grid on, Gen off, GM on, BU off, CH off', 5], ['Grid off, Gen on, GM on, BU off, CH off', 6], ['Grid on, Gen on, GM on, BU off, CH off', 7], ['Grid off, Gen off, GM off, BU on, CH off', 8], ['Grid on, Gen off, GM off, BU on, CH off', 9], ['Grid off, Gen on, GM off, BU on, CH off', 10], ['Grid on, Gen on, GM off, BU on, CH off', 11], ['Grid off, Gen off, GM off, BU off, CH on', 12], ['Grid on, Gen off, GM off, BU off, CH on', 13], ['Grid off, Gen on, GM off, BU off, CH on', 14], ['Grid on, Gen on, GM off, BU off, CH on', 15]], 'icon': 'mdi:numeric', 'category': 'config'},
    'select_charge_timezone3': {'register': 276, 'mask': 15, 'options': [['Grid off, Gen off, GM off, BU off, CH off', 0], ['Grid on, Gen off, GM off, BU off, CH off', 1], ['Grid off, Gen on, GM off, BU off, CH off', 2], ['Grid on, Gen on, GM off, BU off, CH off', 3], ['Grid off, Gen off, GM on, BU off, CH off', 4], ['Grid on, Gen off, GM on, BU off, CH off', 5], ['Grid off, Gen on, GM on, BU off, CH off', 6], ['Grid on, Gen on, GM on, BU off, CH off', 7], ['Grid off, Gen off, GM off, BU on, CH off', 8], ['Grid on, Gen off, GM off, BU on, CH off', 9], ['Grid off, Gen on, GM off, BU on, CH off', 10], ['Grid on, Gen on, GM off, BU on, CH off', 11], ['Grid off, Gen off, GM off, BU off, CH on', 12], ['Grid on, Gen off, GM off, BU off, CH on', 13], ['Grid off, Gen on, GM off, BU off, CH on', 14], ['Grid on, Gen on, GM off, BU off, CH on', 15]], 'icon': 'mdi:numeric', 'category': 'config'},
    'select_charge_timezone4': {'register': 277, 'mask': 15, 'options': [['Grid off, Gen off, GM off, BU off, CH off', 0], ['Grid on, Gen off, GM off, BU off, CH off', 1], ['Grid off, Gen on, GM off, BU off, CH off', 2], ['Grid on, Gen on, GM off, BU off, CH off', 3], ['Grid off, Gen off, GM on, BU off, CH off', 4], ['Grid on, Gen off, GM on, BU off, CH off', 5], ['Grid off, Gen on, GM on, BU off, CH off', 6], ['Grid on, Gen on, GM on, BU off, CH off', 7], ['Grid off, Gen off, GM off, BU on, CH off', 8], ['Grid on, Gen off, GM off, BU on, CH off', 9], ['Grid off, Gen on, GM off, BU on, CH off', 10], ['Grid on, Gen on, GM off, BU on, CH off', 11], ['Grid off, Gen off, GM off, BU off, CH on', 12], ['Grid on, Gen off, GM off, BU off, CH on', 13], ['Grid off, Gen on, GM off, BU off, CH on', 14], ['Grid on, Gen on, GM off, BU off, CH on', 15]], 'icon': 'mdi:numeric', 'category': 'config'},
    'select_charge_timezone5': {'register': 278, 'mask': 15, 'options': [['Grid off, Gen off, GM off, BU off, CH off', 0], ['Grid on, Gen off, GM off, BU off, CH off', 1], ['Grid off, Gen on, GM off, BU off, CH off', 2], ['Grid on, Gen on, GM off, BU off, CH off', 3], ['Grid off, Gen off, GM on, BU off, CH off', 4], ['Grid on, Gen off, GM on, BU off, CH off', 5], ['Grid off, Gen on, GM on, BU off, CH off', 6], ['Grid on, Gen on, GM on, BU off, CH off', 7], ['Grid off, Gen off, GM off, BU on, CH off', 8], ['Grid on, Gen off, GM off, BU on, CH off', 9], ['Grid off, Gen on, GM off, BU on, CH off', 10], ['Grid on, Gen on, GM off, BU on, CH off', 11], ['Grid off, Gen off, GM off, BU off, CH on', 12], ['Grid on, Gen off, GM off, BU off, CH on', 13], ['Grid off, Gen on, GM off, BU off, CH on', 14], ['Grid on, Gen on, GM off, BU off, CH on', 15]], 'icon': 'mdi:numeric', 'category': 'config'},
    'select_charge_timezone6': {'register': 279, 'mask': 15, 'options': [['Grid off, Gen off, GM off, BU off, CH off', 0], ['Grid on, Gen off, GM off, BU off, CH off', 1], ['Grid off, Gen on, GM off, BU off, CH off', 2], ['Grid on, Gen on, GM off, BU off, CH off', 3], ['Grid off, Gen off, GM on, BU off, CH off', 4], ['Grid on, Gen off, GM on, BU off, CH off', 5], ['Grid off, Gen on, GM on, BU off, CH off', 6], ['Grid on, Gen on, GM on, BU off, CH off', 7], ['Grid off, Gen off, GM off, BU on, CH off', 8], ['Grid on, Gen off, GM off, BU on, CH off', 9], ['Grid off, Gen on, GM off, BU on, CH off', 10], ['Grid on, Gen on, GM off, BU on, CH off', 11], ['Grid off, Gen off, GM off, BU off, CH on', 12], ['Grid on, Gen off, GM off, BU off, CH on', 13], ['Grid off, Gen on, GM off, BU off, CH on', 14], ['Grid on, Gen on, GM off, BU off, CH on', 15]], 'icon': 'mdi:numeric', 'category': 'config'},
}

TEXT_SENSORS = {
    'overall_state': {'kind': 'state', 'register': 59, 'bridge': True},
    'time_slot_1': {'kind': 'time', 'register': 250, 'icon': 'mdi:clock', 'bridge': True},
    'time_slot_2': {'kind': 'time', 'register': 251, 'icon': 'mdi:clock'},
    'time_slot_3': {'kind': 'time', 'register': 252, 'icon': 'mdi:clock'},
    'time_slot_4': {'kind': 'time', 'register': 253, 'icon': 'mdi:clock'},
    'time_slot_5': {'kind': 'time', 'register': 254, 'icon': 'mdi:clock'},
    'time_slot_6': {'kind': 'time', 'register': 255, 'icon': 'mdi:clock'},
}

REGISTERS = {
    "sensor": SENSORS,
    "binary_sensor": BINARY_SENSORS,
    "switch": SWITCHES,
    "number": NUMBERS,
    "select": SELECTS,
    "text_sensor": TEXT_SENSORS,
}
