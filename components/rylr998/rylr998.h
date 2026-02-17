#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace rylr998 {

// Forward declarations for optional sensors
namespace sensor {
class RYLR998Sensor;
}

class RYLR998Component : public Component, public uart::UARTDevice {
 public:
  // ── Configuration setters ─────────────────────────────────────────────────
  void set_address(uint16_t address)         { address_      = address; }
  void set_network_id(uint8_t network_id)    { network_id_   = network_id; }
  void set_band(uint32_t band)               { band_         = band; }
  void set_sf(uint8_t sf)                    { sf_           = sf; }
  void set_bw(uint8_t bw)                    { bw_           = bw; }
  void set_cr(uint8_t cr)                    { cr_           = cr; }
  void set_preamble(uint8_t preamble)        { preamble_     = preamble; }
  void set_password(const std::string &pwd)  { password_     = pwd; }
  void set_output_power(uint8_t power)       { output_power_ = power; }

  // ── Sensor registration (called from sensor/__init__.py generated code) ───
  void set_rssi_sensor(sensor::Sensor *s);
  void set_snr_sensor(sensor::Sensor *s);

  // ── ESPHome lifecycle ─────────────────────────────────────────────────────
  void setup()   override;
  void loop()    override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // ── Public TX API ─────────────────────────────────────────────────────────
  bool send(uint16_t dest_addr, const std::string &data);

 protected:
  // ── AT command helpers ────────────────────────────────────────────────────
  void send_at_(const std::string &cmd);
  bool wait_for_ok_(uint32_t timeout_ms = 500);
  void apply_config_();

  // ── RX line parser ────────────────────────────────────────────────────────
  void process_line_(const std::string &line);

  // ── Config fields ─────────────────────────────────────────────────────────
  uint16_t    address_      {0};
  uint8_t     network_id_   {18};
  uint32_t    band_         {868000000};
  uint8_t     sf_           {9};
  uint8_t     bw_           {7};
  uint8_t     cr_           {1};
  uint8_t     preamble_     {12};
  std::string password_     {"FABC0002EEDCAA90"};
  uint8_t     output_power_ {22};

  // ── Runtime state ─────────────────────────────────────────────────────────
  std::string rx_buffer_;

  // ── Optional sensors (nullptr if not declared in YAML) ────────────────────
  esphome::sensor::Sensor *rssi_sensor_ {nullptr};
  esphome::sensor::Sensor *snr_sensor_  {nullptr};
};

}  // namespace rylr998
}  // namespace esphome
