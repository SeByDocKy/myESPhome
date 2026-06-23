#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/automation.h"
#include "mcp2518fd_defs.h"

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace esphome::mcp2518fd {

enum CanClock : uint8_t {
  MCP_40MHZ = 0,
  MCP_20MHZ = 1,
  MCP_10MHZ = 2,
  MCP_4MHZ  = 3,
};

enum CanMode : uint8_t {
  CAN_MODE_NORMAL       = CAN_NORMAL_MODE,
  CAN_MODE_LISTEN_ONLY  = CAN_LISTEN_ONLY_MODE,
  CAN_MODE_LOOPBACK_INT = CAN_INTERNAL_LOOPBACK,
  CAN_MODE_LOOPBACK_EXT = CAN_EXTERNAL_LOOPBACK,
  CAN_MODE_SLEEP        = CAN_SLEEP_MODE,
  CAN_MODE_CLASSIC      = CAN_CLASSIC_MODE,
  CAN_MODE_RESTRICTED   = CAN_RESTRICTED_MODE,
};

// Forward declaration — défini après MCP2518FD ci-dessous
class MCP2518FDCanBusSniffer;

class MCP2518FD : public canbus::Canbus,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                        spi::CLOCK_POLARITY_LOW,
                                        spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_1MHZ> {
 public:
  MCP2518FD() = default;

  // ---- YAML setters --------------------------------------------------------
  void set_can_clock(CanClock clock)           { this->can_clock_     = clock;   }
  void set_mcp_mode(CanMode mode)              { this->mcp_mode_      = mode;    }
  void set_canfd_enabled(bool enabled)         { this->canfd_enabled_ = enabled; }
  void set_can_data_rate(canbus::CanSpeed rate){ this->can_data_rate_  = rate;    }

  // Interrupt pins (tous optionnels, active-low)
  // INT  : interruption générale — toute source activée dans CiINT
  // INT0 : interruption TX      — CiINT.TXIF & TXIE  (broche INT0/GPIO0)
  // INT1 : interruption RX      — CiINT.RXIF & RXIE  (broche INT1/GPIO1)
  void set_int_pin(GPIOPin *pin)  { this->int_pin_  = pin; }
  void set_int0_pin(GPIOPin *pin) { this->int0_pin_ = pin; }
  void set_int1_pin(GPIOPin *pin) { this->int1_pin_ = pin; }

  // Appelé par le framework ESPHome — rapport de statut après connexion WiFi
  void dump_config() override;
  void loop() override;

 protected:
  // ---- Cycle de vie du composant -------------------------------------------
  bool setup_internal() override;

  // ---- Interface canbus::Canbus --------------------------------------------
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

 private:
  // ---- Configuration -------------------------------------------------------
  CanClock         can_clock_     {MCP_40MHZ};
  CanMode          mcp_mode_      {CAN_MODE_NORMAL};
  bool             canfd_enabled_ {false};
  canbus::CanSpeed can_data_rate_ {canbus::CAN_500KBPS};

  // ---- Broches d'interruption (nullptr = non configuré, mode polling) ------
  GPIOPin *int_pin_  {nullptr};  ///< INT  — général   (active-low)
  GPIOPin *int0_pin_ {nullptr};  ///< INT0 — TX done   (active-low)
  GPIOPin *int1_pin_ {nullptr};  ///< INT1 — RX avail  (active-low)

  // ---- Primitives SPI ------------------------------------------------------
  canbus::Error reset_();
  uint32_t      read_sfr_(uint16_t addr);
  void          write_sfr_(uint16_t addr, uint32_t value);
  uint8_t       read_byte_(uint16_t addr);
  void          write_byte_(uint16_t addr, uint8_t value);
  void          read_ram_(uint16_t addr, uint8_t *data, uint8_t n);
  void          write_ram_(uint16_t addr, const uint8_t *data, uint8_t n);

  // ---- Helpers d'initialisation --------------------------------------------
  canbus::Error configure_osc_();
  canbus::Error set_mode_(CanOperationMode mode);
  canbus::Error configure_bit_timing_();
  canbus::Error configure_fifos_();
  canbus::Error configure_filters_();
  canbus::Error configure_interrupts_();

  // ---- Helpers TX / RX -----------------------------------------------------
  canbus::Error send_message_txq_(struct canbus::CanFrame *frame);
  canbus::Error read_message_fifo_(struct canbus::CanFrame *frame);

  uint16_t tx_fifo_addr_();
  uint16_t rx_fifo_addr_();
  void     write_sfr_byte_(uint16_t byte_addr, uint8_t value);  // octet unique, sans RMW

  // ---- Disponibilité RX ----------------------------------------------------
  // Retourne true si une trame RX est en attente.
  // Utilise INT1 si câblé, sinon lit directement le registre de statut FIFO.
  bool rx_available_();

  // ---- Constructeur de commande SPI ----------------------------------------
  inline void build_cmd_(uint16_t addr, uint8_t instr, uint8_t out[2]) {
    out[0] = static_cast<uint8_t>((instr << 4) | ((addr >> 8) & 0x0F));
    out[1] = static_cast<uint8_t>(addr & 0xFF);
  }

  // ---- Diagnostics d'initialisation ----------------------------------------
  uint32_t init_devid_  {0};
  uint32_t init_osc_    {0};
  uint32_t init_cicon_  {0};
  uint32_t init_iocon_  {0};
  uint32_t init_citrec_ {0};
  bool     init_done_   {false};
  uint8_t  rx_frame_count_ {0};  // limite RX par loop()

  // Sniffer text_sensor optionnel (mode LISTEN_ONLY uniquement)
  MCP2518FDCanBusSniffer *sniffer_{nullptr};
 public:
  void set_sniffer(MCP2518FDCanBusSniffer *s) { sniffer_ = s; }
  void drain_to_sniffer_();  // vide tout le FIFO RX vers le sniffer (LISTEN_ONLY seulement)
 private:

  // ---- Utilitaires ---------------------------------------------------------
  static uint8_t dlc_to_bytes_(uint8_t dlc);
  static uint8_t bytes_to_dlc_(uint8_t len);

  bool calc_nbt_(canbus::CanSpeed speed, CanClock clk, uint32_t &nbtcfg) const;
  bool calc_dbt_(canbus::CanSpeed speed, CanClock clk,
                 uint32_t &dbtcfg, uint32_t &tdcval) const;
};

// ============================================================
// MCP2518FDCanBusSniffer — text_sensor affichant les trames CAN en direct.
// Uniquement pertinent en mode LISTEN_ONLY.
// Utilise un map indexé par CAN ID (une entrée par ID, valeur la plus récente).
// Affiche les <frame_displayed> IDs les plus récemment actifs.
// Publie au maximum une fois toutes les 500 ms depuis loop().
// ============================================================
class MCP2518FDCanBusSniffer : public text_sensor::TextSensor, public Component {
 public:
  void set_parent(MCP2518FD *parent) { parent_ = parent; }
  void set_frame_displayed(TemplatableValue<uint8_t> val) { frame_displayed_ = val; }

  void setup() override {}

  void loop() override {
    uint32_t now = millis();
    if (now - last_publish_ms_ < 500)
      return;
    last_publish_ms_ = now;

    if (id_map_.empty())
      return;

    uint8_t max_frames = frame_displayed_.value();
    if (max_frames < 1)  max_frames = 1;
    if (max_frames > 50) max_frames = 50;

    // Copier dans un vecteur et trier par last_ms décroissant (plus récent en premier)
    std::vector<std::pair<uint32_t, SnifferEntry>> entries(id_map_.begin(), id_map_.end());
    std::sort(entries.begin(), entries.end(),
              [](const std::pair<uint32_t, SnifferEntry> &a,
                 const std::pair<uint32_t, SnifferEntry> &b) {
                return a.second.last_ms > b.second.last_ms;
              });

    // Limite stricte HA : 255 caractères par état — on coupe proprement à la frontière d'une ligne
    static const size_t HA_STATE_MAX = 255;

    std::string out;
    uint8_t count = 0;
    for (const auto &kv : entries) {
      if (count >= max_frames) break;
      const std::string &entry_line = kv.second.line;
      // Calculer la taille si on ajoute cette ligne
      size_t needed = out.size() + (out.empty() ? 0 : 1) + entry_line.size();
      if (needed > HA_STATE_MAX) break;   // ne pas dépasser — on arrête ici
      if (!out.empty()) out += "\n";
      out += entry_line;
      ++count;
    }
    publish_state(out);
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  /// Appelé par drain_to_sniffer_() et read_message_fifo_()
  void push_frame(uint32_t can_id, bool extended, const uint8_t *data, uint8_t len) {
    // Format : "0x010A0154: 01 01 32 20 00 00 00 00"
    char line[64];
    if (extended)
      snprintf(line, sizeof(line), "0x%08" PRIX32 ":", can_id);
    else
      snprintf(line, sizeof(line), "0x%03" PRIX32 ":", can_id);
    std::string s(line);
    for (uint8_t i = 0; i < len && i < 8; i++) {
      snprintf(line, sizeof(line), " %02X", data[i]);
      s += line;
    }
    // Stocke par CAN ID — écrase toujours avec la valeur + timestamp actuels
    id_map_[can_id] = {s, millis()};
  }

 protected:
  struct SnifferEntry {
    std::string line;
    uint32_t    last_ms{0};
  };

  MCP2518FD *parent_{nullptr};
  TemplatableValue<uint8_t> frame_displayed_{10};
  std::map<uint32_t, SnifferEntry> id_map_;
  uint32_t last_publish_ms_{0};
};

}  // namespace esphome::mcp2518fd
