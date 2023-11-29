# https://github.com/esphome/esphome/blob/dev/esphome/components/grove_tb6612fng/__init__.py#L176

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

CONF_CURRENT1 = "current1"
CONF_POS_ENERGY1 = "pos_energy1"
CONF_NEG_ENERGY1 = "neg_energy1"
CONF_POWER1 = "power1"
CONF_VOLTAGE1 = "voltage1"
CONF_FREQUENCY1 = "frequency1"
CONF_POWER_FACTOR1 = "power_factor1"

CONF_CURRENT2 = "current2"
CONF_POS_ENERGY2 = "pos_energy2"
CONF_NEG_ENERGY2 = "neg_energy2"
CONF_POWER2 = "power2"
CONF_VOLTAGE2 = "voltage2"
CONF_FREQUENCY2 = "frequency2"
CONF_POWER_FACTOR2 = "power_factor2"

CONF_NEW_ADDRESS = "new_address"
CONF_NEW_BAUDRATE = "new_baudrate"

ICON_FREQUENCY = "mdi:sine-wave"

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["modbus"]

jsy193_ns = cg.esphome_ns.namespace("jsy193")
JSY193 = jsy193_ns.class_("JSY193", cg.PollingComponent, modbus.ModbusDevice)

# Actions
ResetEnergy1Action = jsy193_ns.class_("ResetEnergy1Action", automation.Action)
ResetEnergy2Action = jsy193_ns.class_("ResetEnergy2Action", automation.Action)

ChangeAddressAction = jsy193_ns.class_("ChangeAddressAction" , automation.Action)
ChangeBaudrateAction = jsy193_ns.class_("ChangeBaudrateAction" , automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(JSY193),
            cv.Optional(CONF_VOLTAGE1): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT1): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER1): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POS_ENERGY1): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_NEG_ENERGY1): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_FREQUENCY1): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_FREQUENCY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_FACTOR1): sensor.sensor_schema(
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOLTAGE2): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT2): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER2): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POS_ENERGY2): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_NEG_ENERGY2): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_FREQUENCY2): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_FREQUENCY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_FACTOR2): sensor.sensor_schema(
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

    if CONF_VOLTAGE1 in config:
        conf = config[CONF_VOLTAGE1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_voltage1_sensor(sens))
    if CONF_CURRENT1 in config:
        conf = config[CONF_CURRENT1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_current1_sensor(sens))
    if CONF_POWER1 in config:
        conf = config[CONF_POWER1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power1_sensor(sens))
    if CONF_POS_ENERGY1 in config:
        conf = config[CONF_POS_ENERGY1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pos_energy1_sensor(sens))
    if CONF_NEG_ENERGY1 in config:
        conf = config[CONF_NEG_ENERGY1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_neg_energy1_sensor(sens))    
    if CONF_FREQUENCY1 in config:
        conf = config[CONF_FREQUENCY1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_frequency1_sensor(sens))
    if CONF_POWER_FACTOR1 in config:
        conf = config[CONF_POWER_FACTOR1]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_factor1_sensor(sens))
        
    if CONF_VOLTAGE2 in config:
        conf = config[CONF_VOLTAGE2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_voltage2_sensor(sens))
    if CONF_CURRENT2 in config:
        conf = config[CONF_CURRENT2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_current2_sensor(sens))
    if CONF_POWER2 in config:
        conf = config[CONF_POWER2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power2_sensor(sens))
    if CONF_POS_ENERGY2 in config:
        conf = config[CONF_POS_ENERGY2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_pos_energy2_sensor(sens))
    if CONF_NEG_ENERGY2 in config:
        conf = config[CONF_NEG_ENERGY2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_neg_energy2_sensor(sens))    
    if CONF_FREQUENCY2 in config:
        conf = config[CONF_FREQUENCY2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_frequency2_sensor(sens))
    if CONF_POWER_FACTOR2 in config:
        conf = config[CONF_POWER_FACTOR2]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_factor2_sensor(sens))

@automation.register_action(
    "jsy193.reset_energy1",
    ResetEnergy1Action,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(JSY193),
        }
    ),
)
@automation.register_action(
    "jsy193.reset_energy2",
    ResetEnergy2Action,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(JSY193),
        }
    ),
)

async def reset_energy_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

@automation.register_action(
    "jsy193.change_address",
    ChangeAddressAction,
	cv.Schema(
        {
		  cv.GenerateID(): cv.use_id(JSY193),
          cv.Required(CONF_NEW_ADDRESS): cv.templatable(cv.int_range(min=1, max=255)),
		}
	),
)
async def changeaddress_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var , config[CONF_ID])
    template_address_ = await cg.templatable(config[CONF_NEW_ADDRESS], args, int) 
    return cg.add(var.set_address_(template_address_))
    
@automation.register_action(
    "jsy193.change_baudrate",
    ChangeBaudrateAction,
	cv.Schema(
        {
		  cv.GenerateID(): cv.use_id(JSY193),
          cv.Required(CONF_NEW_BAUDRATE): cv.templatable(cv.int_range(min=3, max=8)),
		}
	),
)
async def changebaudrate_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var , config[CONF_ID])
    template_baudrate_ = await cg.templatable(config[CONF_NEW_BAUDRATE], args, int)
    return cg.add(var.set_baudrate_(template_baudrate_))    
