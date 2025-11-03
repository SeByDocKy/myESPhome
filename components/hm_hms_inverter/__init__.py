from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, binary_sensor, sensor, output, button

from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_SIGNAL_STRENGTH,
    CONF_TEMPERATURE,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_CELSIUS,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
)

_ns = cg.esphome_ns.namespace("hm_hms_inverter")
_cls = _ns.class_("HmHmsPlatform", cg.PollingComponent)
_inv_cls = _ns.class_("HmHmsInverter", cg.Component)
_chan_cls = _ns.class_("HmHmsChannel", cg.Component)

_num_cls = _ns.class_("HmHmsNumber", number.Number)
_but_cls = _ns.class_("HmHmsButton", button.Button, cg.Component)

_percent_cls = _ns.class_("PercentNumber", number.Number, cg.Component)
_absolute_cls = _ns.class_("AbsoluteNumber", number.Number, cg.Component)
_palevel_cls = _ns.class_("PalevelNumber", number.Number, cg.Component)

_out_cls = _ns.class_("PercentFloatOutput", output.FloatOutput)



CODEOWNERS = ["@kvj","@sebydocky"]
DEPENDENCIES = []
AUTO_LOAD = ["sensor", "button", "number", "binary_sensor", "output"]

MULTI_CONF = False

CONF_PINS = "pins"

CONF_CMT_SDIO = "cmt_sdio"
CONF_CMT_CLK = "cmt_clk"
CONF_CMT_CS = "cmt_cs"
CONF_CMT_FCS = "cmt_fcs"
CONF_CMT_GPIO2 = "cmt_gpio2"
CONF_CMT_GPIO3 = "cmt_gpio3"


CONF_NRF_MOSI = "nrf_mosi"
CONF_NRF_MISO = "nrf_miso"
CONF_NRF_CLK = "nrf_clk"
CONF_NRF_CS = "nrf_cs"
CONF_NRF_EN = "nrf_en"
CONF_NRF_IRQ = "nrf_irq"



CONF_DC_CHANNELS = "dc_channels"
CONF_AC_CHANNEL = "ac_channel"
CONF_INVERTER_CHANNEL = "inverter_channel"
CONF_INVERTERS = "inverters"
CONF_SERIAL_NO = "serial"
CONF_RSSI = "rssi"
CONF_LIMIT_PERCENT = "limit_percent"
CONF_LIMIT_ABSOLUTE = "limit_absolute"
CONF_PERCENT_OUTPUT = "percent_output" 
CONF_PALEVEL = "palevel"
CONF_REACHABLE = "reachable"
CONF_RESTART = "restart"

CONF_POWER = "power"
CONF_ENERGY = "energy"
CONF_VOLTAGE = "voltage"
CONF_CURRENT = "current"
CONF_TEMPERATURE = "temperature"

ICON_WIFI = "mdi:wifi-arrow-up-down"

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
    ),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        icon = ICON_THERMOMETER,
        device_class=CONF_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
}).extend(cv.COMPONENT_SCHEMA)

INVERTER_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(_inv_cls),
    cv.Required(CONF_SERIAL_NO): cv.string,
    cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                icon = ICON_WIFI,
                device_class=CONF_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_DC_CHANNELS): [CHANNEL_SCHEMA],
    cv.Optional(CONF_AC_CHANNEL): CHANNEL_SCHEMA,
    cv.Optional(CONF_INVERTER_CHANNEL): CHANNEL_SCHEMA,
    cv.Optional(CONF_PERCENT_OUTPUT): output.FLOAT_OUTPUT_SCHEMA.extend({
        cv.Required(CONF_ID): cv.declare_id(_out_cls),
    }),
    cv.Optional(CONF_RESTART): button.button_schema(_but_cls),
    cv.Optional(CONF_LIMIT_PERCENT): number.number_schema(
        _percent_cls, #_num_cls,
        entity_category="config",
        device_class="power_factor",
        icon="mdi:percent",
        unit_of_measurement="%",
    ),
    cv.Optional(CONF_LIMIT_ABSOLUTE): number.number_schema(
        _absolute_cls, 
        entity_category="config",
        device_class="power",
        icon="mdi:sine-wave",
        unit_of_measurement="W",
    ),
    cv.Optional(CONF_PALEVEL): number.number_schema(
        _palevel_cls, 
        entity_category="config",
        device_class="signal_strength",
        icon="mdi:signal",
        unit_of_measurement="dBm",
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
        
            cv.Required(CONF_CMT_SDIO): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_CMT_CLK): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_CMT_CS): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_CMT_FCS): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_CMT_GPIO2): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_CMT_GPIO3): pins.internal_gpio_input_pin_schema,
            
            cv.Required(CONF_NRF_MOSI): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_NRF_MISO): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_NRF_CLK): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_NRF_CS): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_NRF_EN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_NRF_IRQ): pins.internal_gpio_input_pin_schema,
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
    if conf := config.get(CONF_TEMPERATURE):
        cg.add(var.set_temperature(await sensor.new_sensor(conf)))        

    await cg.register_component(var, config)
    return var


async def to_code(config):
    cg.add_build_flag("-std=c++17")
    cg.add_build_flag("-std=gnu++17")
    cg.add_build_flag("-fexceptions")
    cg.add_build_flag("-DHM_INVERTER")
    cg.add_build_flag("-DHMS_INVERTER")
    
    cg.add_platformio_option("build_unflags", ["-std=gnu++11", "-fno-exceptions"])

    ############# with new OpenDTU lib #############
    # cg.add_library("SPI", None)  ### Works with arduino v2.0.x no more from arduino v3.1.x... 
    # cg.add_library("RF24", None, "https://github.com/nRF24/RF24") # needed for SPImanager version
    # cg.add_library("SpiManager", None, "https://github.com/SeByDocKy/SpiManager") # needed for SPImanager version
    # cg.add_library("CMT2300A", None, "https://github.com/SeByDocKy/CMT2300A") # -> Use new SPImanager framework...
    # cg.add_library("Hoymiles-lib", None, "https://github.com/SeByDocKy/Hoymiles-lib")
    # cg.add_library("Hoymiles", None, "https://github.com/SeByDocKy/Hoymiles") ## new version with SPImanager ####


    ############# With old lib, modified to work properly with ESPhome up to 2024.6.3 , prior to OpenDTU v24.9.26 #############
    cg.add_library("SPI", None)  ### Works with arduino v2.0.x no more from arduino v3.1.x... 
    # cg.add_library("RF24@1.4.9", None) # -> without SPImanager framework...
    cg.add_library("CMT2300A", None, "https://github.com/SeByDocKy/esphome-CMT2300A")
    cg.add_library("Hoymiles-lib", None, "https://github.com/SeByDocKy/Hoymiles-lib")
    cg.add_library("Hoymiles", None, "https://github.com/SeByDocKy/esphome-hoymiles-main") ## former version without SPImanager ####

    ############# With old lib, prior to OpenDTU v24.9.26 #############
    
    
    # cg.add_library("SPI", None)
    # cg.add_library("esphome-hoymiles-libs", None, "https://github.com/nedyarrd/esphome-hoymiles-libs")
    # cg.add_library("Hoymiles", None, "https://github.com/nedyarrd/esphome-hoymiles-main") ## former version without spimanager ####
    # cg.add_library("CMT2300A", None, "https://github.com/nedyarrd/esphome-CMT2300A")


    
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

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

        if CONF_RSSI in inv_conf:
            cg.add(inv_var.set_rssi(await sensor.new_sensor(inv_conf[CONF_RSSI])))
        if CONF_LIMIT_PERCENT in inv_conf:        
            cg.add(inv_var.set_limit_percent_number(await number.new_number(inv_conf[CONF_LIMIT_PERCENT], min_value=0, max_value=100, step=2)))
        if CONF_PALEVEL in inv_conf: 
            cg.add(inv_var.set_palevel_number(await number.new_number(inv_conf[CONF_PALEVEL], min_value=-18, max_value=0, step=1)))    
        if CONF_LIMIT_ABSOLUTE in inv_conf:
            cg.add(inv_var.set_limit_absolute_number(await number.new_number(inv_conf[CONF_LIMIT_ABSOLUTE], min_value=0, max_value=2000, step=20)))
        if CONF_PERCENT_OUTPUT in inv_conf:
            conf = inv_conf[CONF_PERCENT_OUTPUT]
            out = cg.new_Pvariable(conf[CONF_ID])
            await output.register_output(out, conf)
            cg.add(out.set_parent(inv_var))

        if CONF_RESTART in inv_conf:
            btn = await button.new_button(inv_conf[CONF_RESTART])
            cg.add(btn.set_parent(inv_var))
        if CONF_REACHABLE in inv_conf:
            cg.add(inv_var.set_is_reachable_sensor(await binary_sensor.new_binary_sensor(inv_conf[CONF_REACHABLE])))
            
    cg.add(var.set_cmt_sdio(await cg.gpio_pin_expression(config[CONF_PINS][CONF_CMT_SDIO])))
    cg.add(var.set_cmt_clk(await cg.gpio_pin_expression(config[CONF_PINS][CONF_CMT_CLK])))
    cg.add(var.set_cmt_cs(await cg.gpio_pin_expression(config[CONF_PINS][CONF_CMT_CS])))
    cg.add(var.set_cmt_fcs(await cg.gpio_pin_expression(config[CONF_PINS][CONF_CMT_FCS])))
    if CONF_CMT_GPIO2 in config[CONF_PINS]:
      cg.add(var.set_cmt_gpio2(await cg.gpio_pin_expression(config[CONF_PINS][CONF_CMT_GPIO2])))
    if CONF_CMT_GPIO3 in config[CONF_PINS]:
      cg.add(var.set_cmt_gpio3(await cg.gpio_pin_expression(config[CONF_PINS][CONF_CMT_GPIO3])))             

    cg.add(var.set_nrf_mosi(await cg.gpio_pin_expression(config[CONF_PINS][CONF_NRF_MOSI])))
    cg.add(var.set_nrf_miso(await cg.gpio_pin_expression(config[CONF_PINS][CONF_NRF_MISO])))
    cg.add(var.set_nrf_clk(await cg.gpio_pin_expression(config[CONF_PINS][CONF_NRF_CLK])))
    cg.add(var.set_nrf_cs(await cg.gpio_pin_expression(config[CONF_PINS][CONF_NRF_CS])))
    cg.add(var.set_nrf_en(await cg.gpio_pin_expression(config[CONF_PINS][CONF_NRF_EN])))
    cg.add(var.set_nrf_irq(await cg.gpio_pin_expression(config[CONF_PINS][CONF_NRF_IRQ])))




