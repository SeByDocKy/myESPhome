import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins

DEPENDENCIES = []
CODEOWNERS = ["@your_github"]
AUTO_LOAD = []

cmt2300a_ns = cg.esphome_ns.namespace("cmt2300a")
CMT2300AComponent = cmt2300a_ns.class_("CMT2300AComponent", cg.Component)

CONF_SCLK_PIN = "sclk_pin"
CONF_SDIO_PIN = "sdio_pin"
CONF_CS_PIN = "cs_pin"
CONF_FCS_PIN = "fcs_pin"
CONF_GPIO1_PIN = "gpio1_pin"
CONF_GPIO2_PIN = "gpio2_pin"
CONF_GPIO3_PIN = "gpio3_pin"
CONF_FREQUENCY = "frequency"
CONF_DATA_RATE = "data_rate"
CONF_TX_POWER = "tx_power"
CONF_ENABLE_CRC = "enable_crc"

FREQUENCIES = {
    "433MHz": 433000000,
    "868MHz": 868000000,
    "915MHz": 915000000,
}

DATA_RATES = {
    "2kbps": 2000,
    "10kbps": 10000,
    "38.4kbps": 38400,
    "100kbps": 100000,
    "250kbps": 250000,
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CMT2300AComponent),
    cv.Required(CONF_SCLK_PIN): pins.gpio_pin_schema({pins.CONF_OUTPUT: True, pins.CONF_INPUT: False}),
    cv.Required(CONF_SDIO_PIN): pins.gpio_pin_schema({pins.CONF_OUTPUT: True, pins.CONF_INPUT: True}),
    cv.Required(CONF_CS_PIN): pins.gpio_pin_schema({pins.CONF_OUTPUT: True, pins.CONF_INPUT: False}),
    cv.Required(CONF_FCS_PIN): pins.gpio_pin_schema({pins.CONF_OUTPUT: True, pins.CONF_INPUT: False}),
    cv.Optional(CONF_GPIO1_PIN): pins.gpio_pin_schema({pins.CONF_OUTPUT: False, pins.CONF_INPUT: True}),
    cv.Optional(CONF_GPIO2_PIN): pins.gpio_pin_schema({pins.CONF_OUTPUT: True, pins.CONF_INPUT: True}),
    cv.Optional(CONF_GPIO3_PIN): pins.gpio_pin_schema({pins.CONF_OUTPUT: True, pins.CONF_INPUT: True}),
    cv.Optional(CONF_FREQUENCY, default="868MHz"): cv.enum(FREQUENCIES),
    cv.Optional(CONF_DATA_RATE, default="250kbps"): cv.enum(DATA_RATES),
    cv.Optional(CONF_TX_POWER, default=20): cv.int_range(min=0, max=20),
    cv.Optional(CONF_ENABLE_CRC, default=True): cv.bool CMT2300AComponent::configure_frequency_() {
  uint32_t freq_reg = ((uint64_t)this->frequency_ << 16) / 26000000;
  
  this->write_register_(0x10, (freq_reg >> 16) & 0xFF);
  this->write_register_(0x11, (freq_reg >> 8) & 0xFF);
  this->write_register_(0x12, freq_reg & 0xFF);
  
  ESP_LOGD(TAG, "Frequency configured: %u Hz", this->frequency_);
  return true;
}

bool CMT2300AComponent::configure_data_rate_() {
  uint32_t rate_reg = 26000000 / this->data_rate_;
  if (rate_reg > 0xFFFF) rate_reg = 0xFFFF;
  
  this->write_register_(0x20, (rate_reg >> 8) & 0xFF);
  this->write_register_(0x21, rate_reg & 0xFF);
  
  ESP_LOGD(TAG, "Data rate configured: %u bps", this->data_rate_);
  return true;
}

bool CMT2300AComponent::configure_packet_format_() {
  this->write_register_(0x30, 0x8A);  // Variable length, 4 byte preamble, 2 byte sync
  this->write_register_(0x31, this->enable_crc_ ? 0x90 : 0x00);  // CRC-16 + Whitening
  this->write_register_(0x33, 0xAA);  // Sync Word 1
  this->write_register_(0x34, 0x55);  // Sync Word 2
  this->write_register_(0x35, 0x00);  // Address filtering disabled
  this->write_register_(0x36, CMT2300A_FIFO_SIZE);  // Max packet length
  
  ESP_LOGD(TAG, "Packet format configured");
  return true;
}

bool CMT2300AComponent::configure_tx_power_() {
  uint8_t pa_power;
  if (this->tx_power_ <= 0) {
    pa_power = 0x00;
  } else if (this->tx_power_ >= 20) {
    pa_power = 0x3F;
  } else {
    pa_power = (this->tx_power_ * 0x3F) / 20;
  }
  
  this->write_register_(0x40, pa_power);
  
  ESP_LOGD(TAG, "TX Power configured: %d dBm", this->tx_power_);
  return true;
}

bool CMT2300AComponent::calibrate_() {
  ESP_LOGD(TAG, "Starting calibration...");
  
  this->set_mode_(CMT2300A_MODE_CAL);
  
  uint32_t start = millis();
  while ((millis() - start) < 10) {
    if (this->get_mode_() == CMT2300A_MODE_STBY) {
      ESP_LOGD(TAG, "Calibration complete");
      return true;
    }
    delay(1);
  }
  
  ESP_LOGW(TAG, "Calibration timeout");
  return false;
}

uint8_t CMT2300AComponent::get_rssi() {
  return this->read_register_(0x50);
}

}  // namespace cmt2300a

}  // namespace esphome

