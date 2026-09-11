/**
 * ESPHome component for Wi-Fi HaLow (IEEE 802.11ah) via Morse Micro MM6108.
 *
 * Wraps the MM-IoT-SDK to provide HaLow network connectivity within ESPHome.
 * The MM6108 communicates over SPI and requires firmware + BCF loaded at boot.
 *
 * State machine: STOPPED -> CONNECTING -> CONNECTED (with auto-reconnect)
 * SDK callbacks fire from FreeRTOS tasks; we use volatile flags polled in loop().
 */

#include "halow_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

// MM-IoT-SDK headers
extern "C" {
#include "mmhal.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmwlan_stats.h"
#include "mmipal.h"
#ifdef USE_HALOW_REGDB
#include "mmregdb.h"
#else
#include "mmwlan_regdb.def"
#endif
}

// ESP-IDF headers for mDNS integration
extern "C" {
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_private/periph_ctrl.h"
#include "esp_system.h"
#include "soc/periph_defs.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "mdns.h"
// Internal header for esp_netif_obj struct layout — needed to create
// a minimal wrapper around mmipal's raw LWIP netif for mDNS registration.
// The struct layout has been stable since ESP-IDF v4.x.
#include "esp_netif_lwip_internal.h"
}

namespace esphome {
namespace halow {

static const char *const TAG = "halow";

static const uint32_t CONNECT_TIMEOUT_MS = 60000;
static const uint32_t SENSOR_UPDATE_INTERVAL_MS = 10000;

HalowComponent *global_halow_component = nullptr;  // NOLINT

// --- SDK Callbacks and Reconnect Timer ---
// Modeled after Xiao-Halow-to-WiFi-Bridge project which has working reconnection.
// Key insight: reconnect must run from a FreeRTOS timer, NOT from the main loop
// or from within a callback. The 15-second delay lets the SDK clean up internally.

static volatile bool s_link_up = false;
static volatile bool s_link_down_event = false;
static volatile bool s_sta_connected = false;
static volatile bool s_reconnecting = false;
static TimerHandle_t s_reconnect_timer = nullptr;

// Forward declaration
static void do_halow_reconnect(TimerHandle_t t);

static void link_state_cb(enum mmwlan_link_state state, void *arg) {
  s_link_up = (state == MMWLAN_LINK_UP);
  if (state == MMWLAN_LINK_UP) {
    ESP_LOGI(TAG, "HaLow link UP");
  } else {
    s_link_down_event = true;
    ESP_LOGW(TAG, "HaLow link DOWN");
  }
}

// mmipal link status callback — fires on link up/down from LWIP layer
static void mmipal_link_status_cb(const struct mmipal_link_status *status) {
  if (status->link_state == MMIPAL_LINK_UP) {
    s_link_up = true;
    s_reconnecting = false;
    ESP_LOGI(TAG, "mmipal link UP (IP: %s)", status->ip_addr);
  } else {
    s_link_up = false;
    s_link_down_event = true;
    ESP_LOGW(TAG, "mmipal link DOWN");
    // Start reconnect timer (15 seconds) — same approach as Xiao-Halow-to-WiFi-Bridge
    if (!s_reconnecting && s_reconnect_timer != nullptr) {
      if (xTimerStart(s_reconnect_timer, 0) == pdPASS) {
        ESP_LOGI(TAG, "Reconnect scheduled in 15s");
      }
    }
  }
}

static void sta_status_cb(enum mmwlan_sta_state state) {
  const char *states[] = {"DISABLED", "CONNECTING", "CONNECTED"};
  ESP_LOGI(TAG, "STA state: %s", states[state]);
  s_sta_connected = (state == MMWLAN_STA_CONNECTED);
}

// Reconnect handler — runs from FreeRTOS timer task, NOT main loop
static void do_halow_reconnect(TimerHandle_t t) {
  (void) t;
  if (s_link_up) {
    // Already reconnected, nothing to do
    return;
  }

  s_reconnecting = true;
  ESP_LOGW(TAG, "=== RECONNECTING (from timer) ===");

  enum mmwlan_status status = mmwlan_sta_disable();
  if (status != MMWLAN_SUCCESS) {
    ESP_LOGW(TAG, "sta_disable failed (%d), will retry", status);
    s_reconnecting = false;
    // Restart timer to retry
    xTimerStart(s_reconnect_timer, 0);
    return;
  }

  // Re-enable with stored config
  struct mmwlan_sta_args sta_args = MMWLAN_STA_ARGS_INIT;
  auto *comp = esphome::halow::global_halow_component;
  if (comp == nullptr) {
    s_reconnecting = false;
    return;
  }

  sta_args.ssid_len = strlen(comp->get_ssid());
  memcpy(sta_args.ssid, comp->get_ssid(), sta_args.ssid_len);
  sta_args.passphrase_len = strlen(comp->get_password());
  memcpy(sta_args.passphrase, comp->get_password(), sta_args.passphrase_len);
  sta_args.security_type = MMWLAN_SAE;

  // Reset flags
  s_link_up = false;
  s_link_down_event = false;
  s_sta_connected = false;

  ESP_LOGI(TAG, "sta_enable from timer...");
  status = mmwlan_sta_enable(&sta_args, sta_status_cb);
  if (status != MMWLAN_SUCCESS) {
    ESP_LOGW(TAG, "sta_enable failed (%d), retrying in 15s", status);
    s_reconnecting = false;
    xTimerStart(s_reconnect_timer, 0);
    return;
  }
  // If sta_enable succeeds, the mmipal callback will set s_reconnecting = false
  // when link comes up
}

// --- Channel Scan State ---
static std::string s_scan_json;
static volatile int8_t s_scan_noise_dbm = 0;
static volatile bool s_scan_has_noise = false;
static volatile bool s_scan_complete = false;
static volatile bool s_scan_running = false;

// --- Byte Counters (for kbps sensors) ---
// TX: hook netif->output (etharp_output wrapper) to count outgoing bytes.
// RX: hook netif->linkoutput is TX-only. The SDK calls tcpip_input() directly for RX,
//     bypassing netif->input. So for RX bytes we estimate from packet counts × avg size,
//     or we wrap the linkoutput for TX and use a LWIP raw PCB for RX.
//     Simplest correct approach: wrap linkoutput for TX, use pcb recv for RX.
static volatile uint32_t s_tx_bytes = 0;
static volatile uint32_t s_rx_bytes = 0;
static netif_linkoutput_fn s_original_linkoutput = nullptr;

static err_t halow_linkoutput_hook(struct netif *netif, struct pbuf *p) {
  s_tx_bytes += p->tot_len;
  return s_original_linkoutput(netif, p);
}

// For RX, we wrap tcpip_input via --wrap linker flag. The SDK calls tcpip_input()
// directly instead of going through netif->input, so hooking netif->input doesn't work.
extern "C" {
  extern err_t __real_tcpip_input(struct pbuf *p, struct netif *inp);
  err_t __wrap_tcpip_input(struct pbuf *p, struct netif *inp) {
    // Only count bytes for the HaLow netif (name "MM")
    if (inp != nullptr && inp->name[0] == 'M' && inp->name[1] == 'M') {
      s_rx_bytes += p->tot_len;
    }
    return __real_tcpip_input(p, inp);
  }
}

// --- Component Lifecycle ---

float HalowComponent::get_setup_priority() const {
  return setup_priority::WIFI;
}

void HalowComponent::set_manual_ip(const std::string &ip, const std::string &gw,
                                     const std::string &subnet, const std::string &dns1,
                                     const std::string &dns2) {
  this->use_static_ip_ = true;
  this->static_ip_ = ip;
  this->static_gw_ = gw;
  this->static_subnet_ = subnet;
  this->static_dns1_ = dns1;
  this->static_dns2_ = dns2;
}

// Shutdown handler — called before esp_restart() during OTA.
// Properly deinits the WLAN stack and SPI bus so the next boot starts clean.
static void halow_shutdown_handler() {
  ESP_LOGI(TAG, "Shutdown: cleaning up WLAN and SPI...");
  mmwlan_sta_disable();
  mmwlan_shutdown();
  // The SDK's deinit handles spi_bus_remove_device + spi_bus_free internally
  mmwlan_deinit();
  // Also reset the SPI peripheral hardware registers
  periph_module_reset(PERIPH_SPI2_MODULE);
  // Hold MM6108 in reset so it's clean for next boot
  gpio_set_level((gpio_num_t) CONFIG_MM_RESET_N, 0);
  ESP_LOGI(TAG, "Shutdown: cleanup complete");
}

void HalowComponent::setup() {
  ESP_LOGI(TAG, "Setting up Wi-Fi HaLow (MM6108)...");
  global_halow_component = this;

  // Register shutdown handler for clean OTA reboot.
  // This runs before esp_restart() and properly shuts down the WLAN stack + SPI bus.
  esp_register_shutdown_handler(halow_shutdown_handler);

  // Reset SPI peripheral hardware (handles cold boot and cases where
  // shutdown handler didn't run, e.g., watchdog or power-loss reboot)
  periph_module_reset(PERIPH_SPI2_MODULE);
  spi_bus_free(SPI2_HOST);  // Ignore error — may not be initialized

  // Hardware-reset the MM6108 to ensure clean chip state.
  gpio_set_direction((gpio_num_t) this->reset_pin_, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t) this->reset_pin_, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level((gpio_num_t) this->reset_pin_, 1);
  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_LOGI(TAG, "MM6108 hardware reset complete");

  // Initialize MM-IoT-SDK subsystems
  mmhal_init();
  mmwlan_init();

  // Set regulatory domain
  const struct mmwlan_s1g_channel_list *channel_list =
      mmwlan_lookup_regulatory_domain(get_regulatory_db(), this->country_code_.c_str());
  if (channel_list == nullptr) {
    ESP_LOGE(TAG, "Invalid country code: %s", this->country_code_.c_str());
    this->mark_failed();
    return;
  }
  if (mmwlan_set_channel_list(channel_list) != MMWLAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to set channel list");
    this->mark_failed();
    return;
  }

  // Register link state callback
  if (mmwlan_register_link_state_cb(link_state_cb, nullptr) != MMWLAN_SUCCESS) {
    ESP_LOGE(TAG, "Failed to register link state callback");
    this->mark_failed();
    return;
  }

  // Boot the MM6108 (loads firmware + BCF over SPI)
  struct mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
  if (mmwlan_boot(&boot_args) != MMWLAN_SUCCESS) {
    ESP_LOGE(TAG, "MM6108 boot failed");
    this->mark_failed();
    return;
  }

  // Disable power save — keeps radio awake so device responds to ARP/ping reliably.
  // Without this, the radio sleeps between beacons and misses inbound packets.
  mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
  // Sub-band support: when enabled, the rate controller can drop to 1-2 MHz
  // sub-bands for better sensitivity at extreme range. When disabled, forces
  // full operating bandwidth (e.g., 8 MHz) for maximum throughput.
  mmwlan_set_subbands_enabled(this->sub_bands_enabled_);
  ESP_LOGI(TAG, "Power save disabled, sub-bands %s",
           this->sub_bands_enabled_ ? "enabled (max range)" : "disabled (max throughput)");

  // Read and log version info
  struct mmwlan_version version;
  if (mmwlan_get_version(&version) == MMWLAN_SUCCESS) {
    ESP_LOGI(TAG, "Morse FW %s, morselib %s, chip 0x%lx",
             version.morse_fw_version, version.morselib_version, version.morse_chip_id);
  }

  // Read and log MAC address
  uint8_t mac[6];
  if (mmwlan_get_mac_addr(mac) == MMWLAN_SUCCESS) {
    ESP_LOGI(TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  // Initialize IP stack
  struct mmipal_init_args ip_args = MMIPAL_INIT_ARGS_DEFAULT;
  if (this->use_static_ip_) {
    ip_args.mode = MMIPAL_STATIC;
    strncpy(ip_args.ip_addr, this->static_ip_.c_str(), sizeof(ip_args.ip_addr) - 1);
    strncpy(ip_args.netmask, this->static_subnet_.c_str(), sizeof(ip_args.netmask) - 1);
    strncpy(ip_args.gateway_addr, this->static_gw_.c_str(), sizeof(ip_args.gateway_addr) - 1);
    ESP_LOGI(TAG, "Using static IP: %s", this->static_ip_.c_str());
  } else {
    ip_args.mode = MMIPAL_DHCP;
    ESP_LOGI(TAG, "Using DHCP");
  }
  if (mmipal_init(&ip_args) != MMIPAL_SUCCESS) {
    ESP_LOGE(TAG, "IP stack init failed");
    this->mark_failed();
    return;
  }

  // Register mmipal link status callback — this handles reconnection via a FreeRTOS timer.
  // The callback fires on link down, starts a 15-second timer, which then does
  // sta_disable() + sta_enable() from the timer task context (not the main loop).
  mmipal_set_link_status_callback(mmipal_link_status_cb);

  // Create reconnect timer (one-shot, 15 seconds)
  s_reconnect_timer = xTimerCreate("halow_reconn", pdMS_TO_TICKS(15000),
                                   pdFALSE, nullptr, do_halow_reconnect);
  if (s_reconnect_timer == nullptr) {
    ESP_LOGW(TAG, "Could not create reconnect timer");
  }

  this->setup_complete_ = true;
  this->state_ = HalowState::CONNECTING;
  this->start_connect_();
}

void HalowComponent::start_connect_() {
  struct mmwlan_sta_args sta_args = MMWLAN_STA_ARGS_INIT;

  sta_args.ssid_len = this->ssid_.length();
  memcpy(sta_args.ssid, this->ssid_.c_str(), sta_args.ssid_len);

  if (this->security_type_ != "OPEN") {
    sta_args.passphrase_len = this->password_.length();
    memcpy(sta_args.passphrase, this->password_.c_str(), sta_args.passphrase_len);
  }

  if (this->security_type_ == "OWE") {
    sta_args.security_type = MMWLAN_OWE;
  } else if (this->security_type_ == "OPEN") {
    sta_args.security_type = MMWLAN_OPEN;
  } else {
    sta_args.security_type = MMWLAN_SAE;
  }

  // Reset connection state flags
  s_link_up = false;
  s_link_down_event = false;
  s_sta_connected = false;

  ESP_LOGI(TAG, "Connecting to '%s' (%s)...", this->ssid_.c_str(), this->security_type_.c_str());

  enum mmwlan_status status = mmwlan_sta_enable(&sta_args, sta_status_cb);
  if (status != MMWLAN_SUCCESS) {
    ESP_LOGE(TAG, "STA enable failed: %d", status);
    return;
  }

  this->connect_start_time_ = millis();
}

void HalowComponent::full_radio_reset_() {
  // Nuclear reset: completely shut down and reinitialize the WLAN stack.
  // This is needed when the radio is stuck after walking out of range —
  // simple sta_enable() can't recover because the SDK's internal state
  // machine is wedged waiting for a response from a gone AP.
  ESP_LOGW(TAG, "=== FULL RADIO RESET ===");

  // 1. Disable STA (may fail if already in bad state, that's OK)
  mmwlan_sta_disable();

  // 2. Shut down the WLAN stack (powers down the MM6108)
  enum mmwlan_status status = mmwlan_shutdown();
  ESP_LOGI(TAG, "mmwlan_shutdown: %d", status);

  // 3. Hardware reset the MM6108 via RESET_N pin
  gpio_set_level((gpio_num_t) this->reset_pin_, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level((gpio_num_t) this->reset_pin_, 1);
  vTaskDelay(pdMS_TO_TICKS(100));

  // 4. Re-boot the MM6108 (reload firmware + BCF over SPI)
  struct mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
  status = mmwlan_boot(&boot_args);
  if (status != MMWLAN_SUCCESS) {
    ESP_LOGE(TAG, "mmwlan_boot failed after reset: %d", status);
    return;
  }

  // 5. Disable power save again
  mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);

  ESP_LOGI(TAG, "Radio reset complete, ready to reconnect");
}

bool HalowComponent::check_ip_() {
  struct mmipal_ip_config ip_config;
  if (mmipal_get_ip_config(&ip_config) != MMIPAL_SUCCESS) {
    return false;
  }
  if (strcmp(ip_config.ip_addr, "0.0.0.0") == 0) {
    return false;
  }
  network::IPAddress new_ip(ip_config.ip_addr);
  if (!this->ip_addresses_[0].is_set() || this->ip_addresses_[0] != new_ip) {
    this->ip_addresses_[0] = new_ip;
    ESP_LOGI(TAG, "Got IP: %s, Gateway: %s, Netmask: %s",
             ip_config.ip_addr, ip_config.gateway_addr, ip_config.netmask);
  }
  return true;
}

void HalowComponent::loop() {
  if (!this->setup_complete_)
    return;

  switch (this->state_) {
    case HalowState::CONNECTING: {
      // Require STA connected AND valid IP before transitioning.
      // Don't use check_ip_() alone — DHCP lease can be cached from previous session.
      bool sta_ready = s_sta_connected || s_link_up;
      if (sta_ready && this->check_ip_()) {
        // Connected and got IP
        this->state_ = HalowState::CONNECTED;
        if (this->reconnect_count_ > 0) {
          uint32_t downtime_s = (millis() - this->disconnect_time_) / 1000;
          ESP_LOGI(TAG, "=== LINK RECOVERED === after %lus downtime (was attempt %lu)",
                   (unsigned long) downtime_s, this->reconnect_count_);
        }
        this->reconnect_count_ = 0;
        ESP_LOGI(TAG, "HaLow connected to '%s'", this->ssid_.c_str());
        this->last_sensor_update_ = 0;  // Force immediate sensor update
        this->last_rx_check_time_ = millis();  // Reset traffic watchdog
        this->last_rx_packets_ = 0;
        this->weak_rssi_count_ = 0;

        // Ensure the LWIP netif link is marked UP so ARP responses work.
        // Must hold the LWIP core lock — netif_set_link_up asserts this on IDF 5.5.
        {
          uint8_t mac[6];
          mmwlan_get_mac_addr(mac);
          LOCK_TCPIP_CORE();
          for (struct netif *nif = netif_list; nif != nullptr; nif = nif->next) {
            if (memcmp(nif->hwaddr, mac, 6) == 0) {
              if (!netif_is_link_up(nif)) {
                netif_set_link_up(nif);
                ESP_LOGI(TAG, "Forced LWIP netif link UP");
              }
              // Install byte counter hooks (once)
              if (s_original_linkoutput == nullptr && nif->linkoutput != nullptr) {
                // TX: wrap linkoutput to count bytes at the frame level
                s_original_linkoutput = nif->linkoutput;
                nif->linkoutput = halow_linkoutput_hook;
                // RX: handled by __wrap_tcpip_input (linker wrap)
                ESP_LOGI(TAG, "Byte counter hooks installed");
              }
              break;
            }
          }
          UNLOCK_TCPIP_CORE();
        }

        if (!this->mdns_started_) {
          this->start_mdns_();
        }
      } else if (millis() - this->connect_start_time_ > CONNECT_TIMEOUT_MS) {
        // Timeout — the FreeRTOS reconnect timer should be handling this,
        // but if we're stuck in CONNECTING, go to STOPPED to let it retry
        this->reconnect_count_++;
        ESP_LOGW(TAG, "Connect timeout (attempt %lu), waiting for timer retry...",
                 this->reconnect_count_);
        this->connect_start_time_ = millis();
        this->state_ = HalowState::STOPPED;
      }
      break;
    }

    case HalowState::STOPPED: {
      // Wait 5 seconds after disconnect before retrying — gives radio time to settle
      if (this->setup_complete_ && millis() - this->connect_start_time_ > 5000) {
        ESP_LOGI(TAG, "Attempting reconnect...");
        this->state_ = HalowState::CONNECTING;
        this->start_connect_();
      }
      break;
    }

    case HalowState::CONNECTED: {
      // Disconnect detection: mmipal link_status_callback handles reconnection
      // via a FreeRTOS timer (do_halow_reconnect). We just update our state here.
      if (s_link_down_event || s_reconnecting) {
        this->state_ = HalowState::CONNECTING;
        this->ip_addresses_ = {};
        if (s_link_down_event) {
          this->reconnect_count_++;
          this->disconnect_time_ = millis();
          s_link_down_event = false;
          ESP_LOGW(TAG, "=== LINK LOST === (mmipal callback triggered reconnect timer)");
        }
      } else {
        // Update sensors periodically
        uint32_t now = millis();
        if (now - this->last_sensor_update_ > SENSOR_UPDATE_INTERVAL_MS) {
          this->update_sensors_();
          this->last_sensor_update_ = now;
        }
        // Check for scan completion
        if (s_scan_complete && s_scan_running) {
          s_scan_running = false;
          s_scan_complete = false;
          s_scan_json += "]";
#ifdef USE_TEXT_SENSOR
          if (this->scan_results_sensor_ != nullptr) {
            this->scan_results_sensor_->publish_state(s_scan_json);
          }
#endif
#ifdef USE_SENSOR
          if (this->noise_floor_sensor_ != nullptr && s_scan_has_noise) {
            this->noise_floor_sensor_->publish_state((float) s_scan_noise_dbm);
          }
#endif
          s_scan_json.clear();
          ESP_LOGI(TAG, "Scan results published (noise=%d dBm)", (int) s_scan_noise_dbm);
        }
      }
      break;
    }
  }
}

void HalowComponent::update_sensors_() {
#ifdef USE_SENSOR
  if (this->rssi_sensor_ != nullptr) {
    int32_t rssi = mmwlan_get_rssi();
    if (rssi != INT32_MIN) {
      this->rssi_sensor_->publish_state((float) rssi);
    }
  }
  if (this->tx_pps_sensor_ != nullptr || this->rx_pps_sensor_ != nullptr) {
    uint32_t tx = 0, rx = 0;
    mmipal_get_link_packet_counts(&tx, &rx);
    float interval_s = SENSOR_UPDATE_INTERVAL_MS / 1000.0f;
    // Use signed delta to catch counter resets (wraps to negative = skip)
    int32_t tx_delta = (int32_t)(tx - this->prev_tx_packets_);
    int32_t rx_delta = (int32_t)(rx - this->prev_rx_packets_);
    if (this->prev_tx_packets_ > 0 && tx_delta > 0 && tx_delta < 100000) {
      if (this->tx_pps_sensor_ != nullptr)
        this->tx_pps_sensor_->publish_state((float) tx_delta / interval_s);
    }
    if (this->prev_rx_packets_ > 0 && rx_delta > 0 && rx_delta < 100000) {
      if (this->rx_pps_sensor_ != nullptr)
        this->rx_pps_sensor_->publish_state((float) rx_delta / interval_s);
    }
    this->prev_tx_packets_ = tx;
    this->prev_rx_packets_ = rx;
  }
  if (this->tx_kbps_sensor_ != nullptr || this->rx_kbps_sensor_ != nullptr) {
    uint32_t tx_b = s_tx_bytes;
    uint32_t rx_b = s_rx_bytes;
    float interval_s = SENSOR_UPDATE_INTERVAL_MS / 1000.0f;
    int32_t tx_byte_delta = (int32_t)(tx_b - this->prev_tx_bytes_);
    int32_t rx_byte_delta = (int32_t)(rx_b - this->prev_rx_bytes_);
    if (this->prev_tx_bytes_ > 0 && tx_byte_delta > 0 && tx_byte_delta < 10000000) {
      if (this->tx_kbps_sensor_ != nullptr)
        this->tx_kbps_sensor_->publish_state((float) tx_byte_delta * 8.0f / 1000.0f / interval_s);
    }
    if (this->prev_rx_bytes_ > 0 && rx_byte_delta > 0 && rx_byte_delta < 10000000) {
      if (this->rx_kbps_sensor_ != nullptr)
        this->rx_kbps_sensor_->publish_state((float) rx_byte_delta * 8.0f / 1000.0f / interval_s);
    }
    this->prev_tx_bytes_ = tx_b;
    this->prev_rx_bytes_ = rx_b;
  }

  // Rate control stats: find the CURRENTLY active rate by comparing deltas
  // since last update. The rate with the most new packets sent since last
  // check is the one the radio is using right now.
  if (this->mcs_sensor_ != nullptr || this->bandwidth_sensor_ != nullptr ||
      this->tx_success_rate_sensor_ != nullptr) {
    struct mmwlan_rc_stats *rc = mmwlan_get_rc_stats();
    if (rc != nullptr && rc->n_entries > 0) {
      uint32_t n = rc->n_entries;
      if (n > MAX_RC_ENTRIES) n = MAX_RC_ENTRIES;

      // Find rate with most NEW packets since last snapshot
      uint32_t max_delta = 0;
      uint32_t best_idx = 0;
      uint32_t total_delta_sent = 0;
      uint32_t total_delta_success = 0;
      for (uint32_t i = 0; i < n; i++) {
        uint32_t delta = rc->total_sent[i] - this->prev_rc_sent_[i];
        if (delta > max_delta) {
          max_delta = delta;
          best_idx = i;
        }
        total_delta_sent += delta;
        total_delta_success += (rc->total_success[i] - this->prev_rc_success_[i]);
      }

      // Save snapshot for next update
      for (uint32_t i = 0; i < n; i++) {
        this->prev_rc_sent_[i] = rc->total_sent[i];
        this->prev_rc_success_[i] = rc->total_success[i];
      }
      this->prev_rc_count_ = n;

      if (max_delta > 0) {
        uint32_t info = rc->rate_info[best_idx];
        uint8_t bw_raw = (info >> 0) & 0xF;
        uint8_t mcs = (info >> 4) & 0xF;
        uint8_t gi = (info >> 8) & 0x1;

        if (this->mcs_sensor_ != nullptr)
          this->mcs_sensor_->publish_state((float) mcs);
        if (this->bandwidth_sensor_ != nullptr) {
          // BW encoding: 0=1MHz, 1=2MHz, 2=4MHz, 3=8MHz (per 802.11ah)
          float bw_mhz = (bw_raw == 0) ? 1.0f : (bw_raw == 1) ? 2.0f :
                         (bw_raw == 2) ? 4.0f : 8.0f;
          this->bandwidth_sensor_->publish_state(bw_mhz);
        }
        float success_pct = (total_delta_sent > 0)
            ? (100.0f * total_delta_success / total_delta_sent) : 100.0f;
        if (this->tx_success_rate_sensor_ != nullptr && total_delta_sent > 0)
          this->tx_success_rate_sensor_->publish_state(success_pct);

        // Link Quality: human-friendly summary derived from MCS + TX success rate
        // MCS 0-7 indicates radio adaptation, success rate indicates actual delivery
#ifdef USE_TEXT_SENSOR
        if (this->link_quality_sensor_ != nullptr) {
          const char *quality;
          if (mcs >= 5 && success_pct > 95.0f)
            quality = "Excellent";
          else if (mcs >= 3 && success_pct > 90.0f)
            quality = "Good";
          else if (mcs >= 1 && success_pct > 80.0f)
            quality = "Fair";
          else if (success_pct > 50.0f)
            quality = "Poor";
          else
            quality = "Critical";
          this->link_quality_sensor_->publish_state(quality);
        }
#endif
      }
      mmwlan_free_rc_stats(rc);
    }
  }

  // UMAC stats — interference and error indicators
  if (this->tx_frames_dropped_sensor_ != nullptr || this->rx_frames_dropped_sensor_ != nullptr ||
      this->ccmp_failures_sensor_ != nullptr || this->hw_restarts_sensor_ != nullptr) {
    struct mmwlan_stats_umac_data umac_stats;
    if (mmwlan_get_umac_stats(&umac_stats) == MMWLAN_SUCCESS) {
      if (this->tx_frames_dropped_sensor_ != nullptr)
        this->tx_frames_dropped_sensor_->publish_state((float) umac_stats.datapath_txq_frames_dropped);
      if (this->rx_frames_dropped_sensor_ != nullptr)
        this->rx_frames_dropped_sensor_->publish_state((float) umac_stats.datapath_rxq_frames_dropped);
      if (this->ccmp_failures_sensor_ != nullptr)
        this->ccmp_failures_sensor_->publish_state((float) umac_stats.datapath_rx_ccmp_failures);
      if (this->hw_restarts_sensor_ != nullptr)
        this->hw_restarts_sensor_->publish_state((float) umac_stats.hw_restart_counter);
    }
  }
#endif

#ifdef USE_TEXT_SENSOR
  // Dynamic values — update every cycle
  if (this->ip_address_sensor_ != nullptr || this->gateway_sensor_ != nullptr ||
      this->subnet_sensor_ != nullptr) {
    struct mmipal_ip_config ip_config;
    if (mmipal_get_ip_config(&ip_config) == MMIPAL_SUCCESS) {
      if (this->ip_address_sensor_ != nullptr)
        this->ip_address_sensor_->publish_state(ip_config.ip_addr);
      if (this->gateway_sensor_ != nullptr)
        this->gateway_sensor_->publish_state(ip_config.gateway_addr);
      if (this->subnet_sensor_ != nullptr)
        this->subnet_sensor_->publish_state(ip_config.netmask);
    }
  }
  if (this->bssid_sensor_ != nullptr) {
    uint8_t bssid[6];
    if (mmwlan_get_bssid(bssid) == MMWLAN_SUCCESS) {
      char buf[18];
      snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
               bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
      this->bssid_sensor_->publish_state(buf);
    }
  }

  // Static values — publish once
  if (this->ssid_sensor_ != nullptr && this->ssid_sensor_->get_raw_state().empty()) {
    this->ssid_sensor_->publish_state(this->ssid_);
  }
  if (this->mac_address_sensor_ != nullptr && this->mac_address_sensor_->get_raw_state().empty()) {
    uint8_t mac[6];
    if (mmwlan_get_mac_addr(mac) == MMWLAN_SUCCESS) {
      char buf[18];
      snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      this->mac_address_sensor_->publish_state(buf);
    }
  }
  if (this->fw_version_sensor_ != nullptr && this->fw_version_sensor_->get_raw_state().empty()) {
    struct mmwlan_version ver;
    if (mmwlan_get_version(&ver) == MMWLAN_SUCCESS) {
      this->fw_version_sensor_->publish_state(ver.morse_fw_version);
    }
  }
#endif
}

void HalowComponent::start_mdns_() {
  // Find mmipal's LWIP netif by matching HaLow MAC address
  uint8_t mac[6];
  if (mmwlan_get_mac_addr(mac) != MMWLAN_SUCCESS) {
    ESP_LOGW(TAG, "mDNS: could not get MAC address");
    return;
  }

  struct netif *mm_netif = nullptr;
  for (struct netif *nif = netif_list; nif != nullptr; nif = nif->next) {
    if (memcmp(nif->hwaddr, mac, 6) == 0) {
      mm_netif = nif;
      break;
    }
  }
  if (mm_netif == nullptr) {
    ESP_LOGW(TAG, "mDNS: could not find HaLow LWIP netif");
    return;
  }

  // Create a minimal esp_netif_obj wrapper around the raw LWIP netif.
  // mdns_register_netif() stores this pointer and later calls
  // esp_netif_get_ip_info() which reads from esp_netif->lwip_netif.
  auto *fake = (struct esp_netif_obj *) calloc(1, sizeof(struct esp_netif_obj));
  if (fake == nullptr) {
    ESP_LOGW(TAG, "mDNS: alloc failed");
    return;
  }
  fake->lwip_netif = mm_netif;
  fake->flags = (esp_netif_flags_t)(ESP_NETIF_FLAG_AUTOUP);
  fake->if_key = strdup("MM_HALOW_DEF");
  fake->if_desc = strdup("halow");
  fake->route_prio = 50;

  // Initialize mDNS. ESPHome's mdns component may have already tried and failed
  // (because there was no esp_netif registered at that time). We free any failed
  // state and reinitialize.
  mdns_free();  // Clean up ESPHome's failed init attempt
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
    free(fake->if_key);
    free(fake->if_desc);
    free(fake);
    return;
  }

  // Set hostname and instance name
  const char *hostname = App.get_name().c_str();
  mdns_hostname_set(hostname);
  mdns_instance_name_set(hostname);

  // Register our fake netif with mDNS
  err = mdns_register_netif((esp_netif_t *) fake);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS register netif failed: %s", esp_err_to_name(err));
    free(fake->if_key);
    free(fake->if_desc);
    free(fake);
    return;
  }

  // Enable mDNS on this interface
  err = mdns_netif_action((esp_netif_t *) fake, MDNS_EVENT_ENABLE_IP4);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS enable IPv4 failed: %s", esp_err_to_name(err));
  }

  // Announce the IP
  err = mdns_netif_action((esp_netif_t *) fake, MDNS_EVENT_ANNOUNCE_IP4);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS announce failed: %s", esp_err_to_name(err));
  }

  // Re-register ESPHome services. The ESPHome MDNSComponent ran during setup()
  // before we had a network interface, so its mdns_service_add() calls failed.
  // We re-register the _esphomelib._tcp service here so Home Assistant can
  // discover this device.
  // Register _esphomelib._tcp service for Home Assistant discovery
  {
    mdns_txt_item_t txt[] = {
        {(char *) "version", (char *) ESPHOME_VERSION},
        {(char *) "platform", (char *) "ESP32"},
        {(char *) "board", (char *) ESPHOME_BOARD},
        {(char *) "network", (char *) "halow"},
    };
    err = mdns_service_add(nullptr, "_esphomelib", "_tcp", 6053, txt, 4);
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "mDNS: registered _esphomelib._tcp service");
    } else {
      ESP_LOGW(TAG, "mDNS: service add failed: %s", esp_err_to_name(err));
    }
  }

  ESP_LOGI(TAG, "mDNS: registered '%s.local' on HaLow interface", hostname);
  this->mdns_started_ = true;
}

void HalowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Wi-Fi HaLow (MM6108):");
  ESP_LOGCONFIG(TAG, "  SSID: '%s'", this->ssid_.c_str());
  ESP_LOGCONFIG(TAG, "  Security: %s", this->security_type_.c_str());
  ESP_LOGCONFIG(TAG, "  Country: %s", this->country_code_.c_str());
  ESP_LOGCONFIG(TAG, "  SPI Pins: CLK=%d MOSI=%d MISO=%d CS=%d",
                this->spi_clk_pin_, this->spi_mosi_pin_,
                this->spi_miso_pin_, this->spi_cs_pin_);
  ESP_LOGCONFIG(TAG, "  Control Pins: RESET=%d WAKE=%d IRQ=%d BUSY=%d",
                this->reset_pin_, this->wake_pin_,
                this->spi_irq_pin_, this->busy_pin_);
  if (this->use_static_ip_) {
    ESP_LOGCONFIG(TAG, "  Static IP: %s", this->static_ip_.c_str());
    ESP_LOGCONFIG(TAG, "  Gateway: %s", this->static_gw_.c_str());
    ESP_LOGCONFIG(TAG, "  Subnet: %s", this->static_subnet_.c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  IP: DHCP");
  }
  if (this->is_connected()) {
    ESP_LOGCONFIG(TAG, "  Current IP: %s", this->get_ip_address_str().c_str());
  }
}

std::string HalowComponent::get_ip_address_str() const {
  if (this->ip_addresses_[0].is_set()) {
    char buf[64];
    this->ip_addresses_[0].str_to(buf);
    return std::string(buf);
  }
  return "0.0.0.0";
}

// --- Channel Scan ---

static void halow_scan_rx_cb(const struct mmwlan_scan_result *result, void *arg) {
  char bssid[18];
  snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
           result->bssid[0], result->bssid[1], result->bssid[2],
           result->bssid[3], result->bssid[4], result->bssid[5]);

  char ssid[33] = {0};
  if (result->ssid_len > 0 && result->ssid_len <= 32) {
    memcpy(ssid, result->ssid, result->ssid_len);
  }

  // Build JSON entry
  char entry[200];
  snprintf(entry, sizeof(entry),
           "{\"ssid\":\"%s\",\"bssid\":\"%s\",\"rssi\":%d,\"noise\":%d,\"freq\":%lu,\"bw\":%u}",
           ssid, bssid, (int) result->rssi, (int) result->noise_dbm,
           (unsigned long) (result->channel_freq_hz / 1000), result->bw_mhz);

  if (s_scan_json.length() > 1) {
    s_scan_json += ",";
  }
  s_scan_json += entry;

  // Track noise floor from our AP's channel (use the best noise reading)
  if (!s_scan_has_noise || result->noise_dbm > s_scan_noise_dbm) {
    // Higher noise = worse, but we want the reading from our channel.
    // Just take the last reading — typically there's one AP on our channel.
    s_scan_noise_dbm = result->noise_dbm;
    s_scan_has_noise = true;
  }

  ESP_LOGD(TAG, "Scan: %s (%s) RSSI=%d noise=%d freq=%luHz bw=%uMHz",
           ssid, bssid, (int) result->rssi, (int) result->noise_dbm,
           (unsigned long) result->channel_freq_hz, result->bw_mhz);
}

static void halow_scan_complete_cb(enum mmwlan_scan_state state, void *arg) {
  ESP_LOGI(TAG, "Channel scan complete (state=%d)", (int) state);
  s_scan_complete = true;
}

void HalowComponent::request_channel_scan() {
  if (s_scan_running) {
    ESP_LOGW(TAG, "Scan already in progress");
    return;
  }
  if (this->state_ != HalowState::CONNECTED) {
    ESP_LOGW(TAG, "Cannot scan — not connected");
    return;
  }

  ESP_LOGI(TAG, "Starting channel scan...");
  s_scan_json = "[";
  s_scan_noise_dbm = 0;
  s_scan_has_noise = false;
  s_scan_complete = false;
  s_scan_running = true;

  struct mmwlan_scan_req scan_req = MMWLAN_SCAN_REQ_INIT;
  scan_req.scan_rx_cb = halow_scan_rx_cb;
  scan_req.scan_complete_cb = halow_scan_complete_cb;
  scan_req.scan_cb_arg = nullptr;

  enum mmwlan_status status = mmwlan_scan_request(&scan_req);
  if (status != MMWLAN_SUCCESS) {
    ESP_LOGW(TAG, "Scan request failed: %d", (int) status);
    s_scan_running = false;
    s_scan_json.clear();
  }
}

}  // namespace halow
}  // namespace esphome
