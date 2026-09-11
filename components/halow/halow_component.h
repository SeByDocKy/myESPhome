#pragma once

#include "esphome/core/component.h"
#include "esphome/components/network/ip_address.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

#include <string>
#include <array>

namespace esphome {
namespace halow {

enum class HalowState : uint8_t {
  STOPPED,
  CONNECTING,
  CONNECTED,
};

class HalowComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  bool is_connected() const { return this->state_ == HalowState::CONNECTED; }
  const char *get_ssid() const { return this->ssid_.c_str(); }
  const char *get_password() const { return this->password_.c_str(); }
  network::IPAddresses get_ip_addresses() const { return this->ip_addresses_; }
  std::string get_ip_address_str() const;

  // Configuration setters
  void set_ssid(const std::string &ssid) { this->ssid_ = ssid; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_country_code(const std::string &cc) { this->country_code_ = cc; }
  void set_security_type(const std::string &type) { this->security_type_ = type; }
  void set_sub_bands_enabled(bool enabled) { this->sub_bands_enabled_ = enabled; }

  void set_manual_ip(const std::string &ip, const std::string &gw,
                     const std::string &subnet, const std::string &dns1,
                     const std::string &dns2);

  void set_spi_clk_pin(uint8_t pin) { this->spi_clk_pin_ = pin; }
  void set_spi_mosi_pin(uint8_t pin) { this->spi_mosi_pin_ = pin; }
  void set_spi_miso_pin(uint8_t pin) { this->spi_miso_pin_ = pin; }
  void set_spi_cs_pin(uint8_t pin) { this->spi_cs_pin_ = pin; }
  void set_spi_irq_pin(uint8_t pin) { this->spi_irq_pin_ = pin; }
  void set_reset_pin(uint8_t pin) { this->reset_pin_ = pin; }
  void set_wake_pin(uint8_t pin) { this->wake_pin_ = pin; }
  void set_busy_pin(uint8_t pin) { this->busy_pin_ = pin; }

  // Sensor setters
#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
  void set_tx_pps_sensor(sensor::Sensor *s) { this->tx_pps_sensor_ = s; }
  void set_rx_pps_sensor(sensor::Sensor *s) { this->rx_pps_sensor_ = s; }
  void set_tx_kbps_sensor(sensor::Sensor *s) { this->tx_kbps_sensor_ = s; }
  void set_rx_kbps_sensor(sensor::Sensor *s) { this->rx_kbps_sensor_ = s; }
  void set_mcs_sensor(sensor::Sensor *s) { this->mcs_sensor_ = s; }
  void set_bandwidth_sensor(sensor::Sensor *s) { this->bandwidth_sensor_ = s; }
  void set_tx_success_rate_sensor(sensor::Sensor *s) { this->tx_success_rate_sensor_ = s; }
  void set_noise_floor_sensor(sensor::Sensor *s) { this->noise_floor_sensor_ = s; }
  void set_tx_frames_dropped_sensor(sensor::Sensor *s) { this->tx_frames_dropped_sensor_ = s; }
  void set_rx_frames_dropped_sensor(sensor::Sensor *s) { this->rx_frames_dropped_sensor_ = s; }
  void set_ccmp_failures_sensor(sensor::Sensor *s) { this->ccmp_failures_sensor_ = s; }
  void set_hw_restarts_sensor(sensor::Sensor *s) { this->hw_restarts_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_ip_address_sensor(text_sensor::TextSensor *s) { this->ip_address_sensor_ = s; }
  void set_gateway_sensor(text_sensor::TextSensor *s) { this->gateway_sensor_ = s; }
  void set_subnet_sensor(text_sensor::TextSensor *s) { this->subnet_sensor_ = s; }
  void set_ssid_sensor(text_sensor::TextSensor *s) { this->ssid_sensor_ = s; }
  void set_bssid_sensor(text_sensor::TextSensor *s) { this->bssid_sensor_ = s; }
  void set_mac_address_sensor(text_sensor::TextSensor *s) { this->mac_address_sensor_ = s; }
  void set_fw_version_sensor(text_sensor::TextSensor *s) { this->fw_version_sensor_ = s; }
  void set_link_quality_sensor(text_sensor::TextSensor *s) { this->link_quality_sensor_ = s; }
  void set_scan_results_sensor(text_sensor::TextSensor *s) { this->scan_results_sensor_ = s; }
#endif
#ifdef USE_BUTTON
  // Button stored for reference but not used — HalowScanButton calls request_channel_scan() directly
  void set_scan_button(button::Button *b) { (void) b; }
#endif

  // Called by the scan button
  void request_channel_scan();

 protected:
  void start_connect_();
  bool check_ip_();
  void update_sensors_();
  void start_mdns_();
  void full_radio_reset_();

  // Config
  std::string ssid_;
  std::string password_;
  std::string country_code_{"US"};
  std::string security_type_{"SAE"};
  bool sub_bands_enabled_{false};

  // Static IP
  bool use_static_ip_{false};
  std::string static_ip_;
  std::string static_gw_;
  std::string static_subnet_;
  std::string static_dns1_;
  std::string static_dns2_;

  // SPI pins
  uint8_t spi_clk_pin_{7};
  uint8_t spi_mosi_pin_{9};
  uint8_t spi_miso_pin_{8};
  uint8_t spi_cs_pin_{4};
  uint8_t spi_irq_pin_{3};
  uint8_t reset_pin_{1};
  uint8_t wake_pin_{2};
  uint8_t busy_pin_{5};

  // State
  HalowState state_{HalowState::STOPPED};
  uint32_t connect_start_time_{0};
  uint32_t last_sensor_update_{0};
  uint32_t reconnect_count_{0};
  uint32_t disconnect_time_{0};
  uint32_t last_rx_packets_{0};
  uint32_t last_rx_check_time_{0};
  uint8_t weak_rssi_count_{0};
  bool setup_complete_{false};
  bool mdns_started_{false};
  network::IPAddresses ip_addresses_{};

  // Sensors
#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
  sensor::Sensor *tx_pps_sensor_{nullptr};
  sensor::Sensor *rx_pps_sensor_{nullptr};
  sensor::Sensor *tx_kbps_sensor_{nullptr};
  sensor::Sensor *rx_kbps_sensor_{nullptr};
  uint32_t prev_tx_packets_{0};
  uint32_t prev_rx_packets_{0};
  uint32_t prev_tx_bytes_{0};
  uint32_t prev_rx_bytes_{0};
  sensor::Sensor *mcs_sensor_{nullptr};
  sensor::Sensor *bandwidth_sensor_{nullptr};
  sensor::Sensor *tx_success_rate_sensor_{nullptr};
  sensor::Sensor *noise_floor_sensor_{nullptr};
  sensor::Sensor *tx_frames_dropped_sensor_{nullptr};
  sensor::Sensor *rx_frames_dropped_sensor_{nullptr};
  sensor::Sensor *ccmp_failures_sensor_{nullptr};
  sensor::Sensor *hw_restarts_sensor_{nullptr};
  // Previous rc_stats snapshot for delta computation
  static const uint8_t MAX_RC_ENTRIES = 32;
  uint32_t prev_rc_sent_[MAX_RC_ENTRIES]{};
  uint32_t prev_rc_success_[MAX_RC_ENTRIES]{};
  uint32_t prev_rc_count_{0};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *ip_address_sensor_{nullptr};
  text_sensor::TextSensor *gateway_sensor_{nullptr};
  text_sensor::TextSensor *subnet_sensor_{nullptr};
  text_sensor::TextSensor *ssid_sensor_{nullptr};
  text_sensor::TextSensor *bssid_sensor_{nullptr};
  text_sensor::TextSensor *mac_address_sensor_{nullptr};
  text_sensor::TextSensor *fw_version_sensor_{nullptr};
  text_sensor::TextSensor *link_quality_sensor_{nullptr};
  text_sensor::TextSensor *scan_results_sensor_{nullptr};
#endif
};

extern HalowComponent *global_halow_component;  // NOLINT

#ifdef USE_BUTTON
class HalowScanButton : public button::Button {
 public:
  void set_parent(HalowComponent *parent) { this->parent_ = parent; }
  void press_action() override { this->parent_->request_channel_scan(); }

 protected:
  HalowComponent *parent_{nullptr};
};
#endif

}  // namespace halow
}  // namespace esphome
