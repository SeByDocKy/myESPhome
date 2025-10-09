from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, binary_sensor, sensor

from esphome.const import (
    CONF_ID,
    CONF_NAME,
)

_ns = cg.esphome_ns.namespace("hoymiles_inverter")
_cls = _ns.class_("HoymilesPlatform", cg.PollingComponent)
_inv_cls = _ns.class_("HoymilesInverter", cg.Component)
_chan_cls = _ns.class_("HoymilesChannel", cg.Component)
_num_cls = _ns.class_("HoymilesNumber", number.Number)

CODEOWNERS = ["@kvj"]
DEPENDENCIES = []
AUTO_LOAD = ["sensor", "number", "binary_sensor"]

MULTI_CONF = False

CONF_PINS = "pins"
CONF_SDIO = "sdio"
CONF_CLK = "clk"
CONF_CS = "cs"
CONF_FCS = "fcs"
CONF_GPIO2 = "gpio2"
CONF_GPIO3 = "gpio3"
CONF_DC_CHANNELS = "dc_channels"
CONF_AC_CHANNEL = "ac_channel"
CONF_INVERTER_CHANNEL = "inverter_channel"
CONF_INVERTERS = "inverters"
CONF_SERIAL_NO = "serial"
CONF_LIMIT_PERCENT = "limit_percent"
CONF_LIMIT_ABSOLUTE = "limit_absolute"
CONF_REACHABLE = "reachable"

CONF_POWER = "power"
CONF_ENERGY = "energy"
CONF_VOLTAGE = "voltage"
CONF_CURRENT = "current"

CHANNEL_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(_chan_cls),
    cv.Optional(CONF_POWER): sensor.sensor_schema(
        unit_of_measurement="W",
        accuracy_decimals=0,
        device_class="power",
        state_class="measurement",
    ),
    cv.Optional(CONF_ENERGY): sensor.sensor_schema(
        unit_of_measurement="Wh",
        accuracy_decimals=1,
        device_class="energy",
        state_class="total_increasing",
    ),
    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
        unit_of_measurement="V",
        accuracy_decimals=1,
        device_class="voltage",
        state_class="measurement",
    ),
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(
        unit_of_measurement="A",
        accuracy_decimals=1,
        device_class="current",
        state_class="measurement",
    )
}).extend(cv.COMPONENT_SCHEMA)

INVERTER_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(_inv_cls),
    cv.Required(CONF_SERIAL_NO): cv.string,
    cv.Optional(CONF_DC_CHANNELS): [CHANNEL_SCHEMA],
    cv.Optional(CONF_AC_CHANNEL): CHANNEL_SCHEMA,
    cv.Optional(CONF_INVERTER_CHANNEL): CHANNEL_SCHEMA,
    cv.Optional(CONF_LIMIT_PERCENT): number.number_schema(
        _num_cls,
        entity_category="config",
        device_class="power_factor",
        unit_of_measurement="%",
    ),
    cv.Optional(CONF_LIMIT_ABSOLUTE): number.number_schema(
        _num_cls,
        entity_category="config",
        device_class="power",
        unit_of_measurement="W",
    ),
    cv.Optional(CONF_REACHABLE): binary_sensor.binary_sensor_schema(
        binary_sensor.BinarySensorInitiallyOff,
        entity_category="diagnostic",
        device_class="connectivity",
    )
}).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(_cls),
        cv.Required(CONF_PINS): cv.Schema({
            cv.Required(CONF_SDIO): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_CLK): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_CS): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_FCS): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_GPIO2): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_GPIO3): pins.internal_gpio_input_pin_schema,
        }),
        cv.Required(CONF_INVERTERS): [INVERTER_SCHEMA],
    })
    .extend(cv.polling_component_schema("10s"))
)

async def channel_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    if conf := config.get(CONF_POWER):
        cg.add(var.set_power_sensor(await sensor.new_sensor(conf)))
    if conf := config.get(CONF_ENERGY):
        cg.add(var.set_energy_sensor(await sensor.new_sensor(conf)))
    if conf := config.get(CONF_VOLTAGE):
        cg.add(var.set_voltage_sensor(await sensor.new_sensor(conf)))
    if conf := config.get(CONF_CURRENT):
        cg.add(var.set_current_sensor(await sensor.new_sensor(conf)))

    await cg.register_component(var, config)
    return var


async def to_code(config):
    cg.add_build_flag("-std=c++17")
    cg.add_build_flag("-std=gnu++17")
    cg.add_build_flag("-fexceptions")
    cg.add_platformio_option("build_unflags", ["-std=gnu++11", "-fno-exceptions"])

    ############# new lib #############
    # cg.add_library("SPI", None)
    # cg.add_library("RF24", None, "https://github.com/nRF24/RF24")
    # cg.add_library("SpiManager", None, "https://github.com/SeByDocKy/SpiManager") 
    # cg.add_library("CMT2300A", None, "https://github.com/SeByDocKy/CMT2300A")
    # cg.add_library("Hoymiles-lib", None, "https://github.com/SeByDocKy/Hoymiles-lib")
    # cg.add_library("Hoymiles", None, "https://github.com/SeByDocKy/Hoymiles")

     ############# old lib #############
    # cg.add_library("lib/OpenDTU",None,"https://github.com/tbnobody/OpenDTU@1f1227fa104cda435df7132ddfa011524a262554")
    cg.add_library("SPI", None)
    cg.add_library("esphome-hoymiles-libs", None, "https://github.com/nedyarrd/esphome-hoymiles-libs")
    cg.add_library("Hoymiles", None, "https://github.com/nedyarrd/esphome-hoymiles-main")
    cg.add_library("CMT2300A", None, "https://github.com/nedyarrd/esphome-CMT2300A")


    
    var = cg.new_Pvariable(config[CONF_ID])

    for inv_conf in config[CONF_INVERTERS]:
        inv_var = cg.new_Pvariable(inv_conf[CONF_ID])
        cg.add(inv_var.set_serial_no(inv_conf[CONF_SERIAL_NO]))

        for conf in inv_conf.get(CONF_DC_CHANNELS, []):
            cg.add(inv_var.add_channel(await channel_to_code(conf)))
        if conf := inv_conf.get(CONF_AC_CHANNEL):
            cg.add(inv_var.set_ac_channel(await channel_to_code(conf)))
        if conf := inv_conf.get(CONF_INVERTER_CHANNEL):
            cg.add(inv_var.set_inverter_channel(await channel_to_code(conf)))
            
        await cg.register_component(inv_var, inv_conf)
        cg.add(var.add_inverter(inv_var))

        if CONF_LIMIT_PERCENT in inv_conf:
            cg.add(inv_var.set_limit_percent_number(await number.new_number(inv_conf[CONF_LIMIT_PERCENT], min_value=0, max_value=100, step=10)))
        if CONF_LIMIT_ABSOLUTE in inv_conf:
            cg.add(inv_var.set_limit_absolute_number(await number.new_number(inv_conf[CONF_LIMIT_ABSOLUTE], min_value=0, max_value=2500, step=10)))
        if CONF_REACHABLE in inv_conf:
            cg.add(inv_var.set_is_reachable_sensor(await binary_sensor.new_binary_sensor(inv_conf[CONF_REACHABLE])))

    cg.add(var.set_pins(
        await cg.gpio_pin_expression(config[CONF_PINS][CONF_SDIO]),
        await cg.gpio_pin_expression(config[CONF_PINS][CONF_CLK]),
        await cg.gpio_pin_expression(config[CONF_PINS][CONF_CS]),
        await cg.gpio_pin_expression(config[CONF_PINS][CONF_FCS]),
        await cg.gpio_pin_expression(config[CONF_PINS][CONF_GPIO2]),
        await cg.gpio_pin_expression(config[CONF_PINS][CONF_GPIO3]),
    ))
    await cg.register_component(var, config)
