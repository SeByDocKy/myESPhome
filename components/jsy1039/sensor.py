import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import sensor, modbus
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_ENERGY,
    ICON_CURRENT_AC,
    ICON_POWER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
    UNIT_KILOWATT_HOURS,
)

CONF_CURRENT = "current"
CONF_POS_ENERGY = "pos_energy"
CONF_NEG_ENERGY = "neg_energy"
CONF_POWER = "power"
CONF_VOLTAGE = "voltage"
CONF_FREQUENCY = "frequency"
CONF_POWER_FACTOR = "power_factor"

CONF_NEW_ADDRESS = "new_address"
CONF_NEW_BAUDRATE = "new_baudrate"

ICON_FREQUENCY = "mdi:sine-wave"

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["modbus"]

jsy1039_ns = cg.esphome_ns.namespace("jsy1039")
JSY1039 = jsy1039_ns.class_("JSY1039", cg.PollingComponent, modbus.ModbusDevice)

# Actions
ResetEnergyAction = jsy1039_ns.class_("ResetEnergyAction", automation.Action)
WriteCommunicationSettingAction = jsy1039_ns.class_("WriteCommunicationSettingAction" , automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(JSY1039),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POS_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_NEG_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_FREQUENCY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(modbus.modbus_device_schema(0x01))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)

    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_voltage_sensor(sens))
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_current_sensor(sens))
    if CONF_POWER in config:
        conf = config[CONF_POWER]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_sensor(sens))
    if CONF_POS_ENERGY in config:
        conf = config[CONF_POS_ENERGY]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pos_energy_sensor(sens))
    if CONF_NEG_ENERGY in config:
        conf = config[CONF_NEG_ENERGY]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_neg_energy_sensor(sens))    
    if CONF_FREQUENCY in config:
        conf = config[CONF_FREQUENCY]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_frequency_sensor(sens))
    if CONF_POWER_FACTOR in config:
        conf = config[CONF_POWER_FACTOR]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_factor_sensor(sens))
        

@automation.register_action(
    "jsy1039.reset_energy",
    ResetEnergyAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(JSY1039),
        }
    ),
)


async def reset_energy_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "jsy1039.write_com_setting",
    WriteCommunicationSettingAction,
	cv.Schema(
        {
          cv.GenerateID(): cv.use_id(JSY1039),
          cv.Required(CONF_NEW_ADDRESS): cv.templatable(cv.int_range(min=1, max=255)),
          cv.Required(CONF_NEW_BAUDRATE): cv.templatable(cv.int_range(min=3, max=8)),
		}
	),
)
	
async def writecommunicationsetting_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg , parent)	
    template_address = await cg.templatable(config[CONF_NEW_ADDRESS], args, int)
    template_baudrate = await cg.templatable(config[CONF_NEW_BAUDRATE], args, int)
    cg.add(var.set_new_address(template_address))
    cg.add(var.set_new_baudrate(template_baudrate))
    return var
