#pragma once

#include <vector>
#include <string>
#include <cstring>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/cmt2300a/cmt2300a.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace hms {

// ---------------------------------------------------------------------------
// Types portés depuis OpenDTU lib/Hoymiles/src/parser/StatisticsParser.h
// (uniquement le sous-ensemble utilisé par les onduleurs HMS 1/2/4 canaux)
// ---------------------------------------------------------------------------
enum FieldId_t : uint8_t {
  FLD_UDC = 0,
  FLD_IDC,
  FLD_PDC,
  FLD_YD,
  FLD_YT,
  FLD_UAC,
  FLD_IAC,
  FLD_PAC,
  FLD_F,
  FLD_T,
  FLD_PF,
  FLD_EFF,
  FLD_IRR,
  FLD_Q,
  FLD_EVT_LOG,
};

enum ChannelType_t : uint8_t { TYPE_AC = 0, TYPE_DC, TYPE_INV };
enum ChannelNum_t : uint8_t { CH0 = 0, CH1, CH2, CH3 };

// Indices des fonctions de calcul (quand div == CMD_CALC)
enum {
  CALC_TOTAL_YT = 0,
  CALC_TOTAL_YD,
  CALC_TOTAL_PDC,
  CALC_TOTAL_EFF,
  CALC_CH_IRR,
};
static const uint16_t CMD_CALC = 0xFFFF;

struct byteAssign_t {
  ChannelType_t type;
  ChannelNum_t ch;
  FieldId_t fieldId;
  uint8_t start;   // position du 1er octet dans le buffer (ou index calc si div==CMD_CALC)
  uint8_t num;     // nombre d'octets (ou argument de la fonction calc)
  uint16_t div;    // diviseur, ou CMD_CALC
  bool isSigned;
  uint8_t digits;
};

static const uint8_t STATISTIC_PACKET_SIZE = 7 * 16;  // 112 octets, comme OpenDTU

// ---------------------------------------------------------------------------
// Fréquence / pays -- porté depuis HoymilesRadio_CMT.h/cpp
// ---------------------------------------------------------------------------
enum class FrequencyBand : uint8_t { EU_860 = 0, US_900 };

struct FreqDef {
  uint32_t base_freq;      // Hz -- base de la bande (860 ou 900 MHz)
  uint32_t freq_startup;   // Hz -- fréquence de "boot" de l'onduleur après coupure
  uint32_t freq_default;   // Hz -- fréquence de travail par défaut du DTU
};

static const uint32_t CMT_ONE_STEP_SIZE = 2500;  // Hz, un pas = 2.5 kHz
static const uint8_t FH_OFFSET = 100;            // pas * FH_OFFSET = largeur de canal
// getChannelWidth() = FH_OFFSET * CMT_ONE_STEP_SIZE = 250 kHz

// ---------------------------------------------------------------------------
// Fragment RF brut (32 octets max), porté depuis types.h
// ---------------------------------------------------------------------------
struct fragment_t {
  uint8_t mainCmd{0};
  uint8_t fragment[32]{};
  uint8_t len{0};
  bool wasReceived{false};
};

static const uint8_t MAX_RF_FRAGMENT_COUNT = 13;

enum RadioOpState : uint8_t {
  OP_IDLE = 0,
  OP_WAIT_RESPONSE,
};

// Identifiant logique de la commande actuellement en vol (pour savoir comment
// interpréter la réponse / quoi faire au timeout)
enum PendingCmd : uint8_t {
  CMD_NONE = 0,
  CMD_REALTIME_DATA,
  CMD_ACTIVE_POWER_CONTROL,
  CMD_CHANNEL_CHANGE,
};

enum PowerLimitType : uint8_t {
  POWER_ABSOLUTE = 0,
  POWER_RELATIVE = 1,
};

class HMSComponent : public Component {
 public:
  void set_radio(cmt2300a::CMT2300AComponent *radio) { this->radio_ = radio; }
  void set_inverter_serial(uint64_t serial) { this->inverter_serial_ = serial; }
  void set_dtu_serial(uint64_t serial) { this->dtu_serial_ = serial; }
  void set_frequency_band(uint8_t band) { this->frequency_band_ = static_cast<FrequencyBand>(band); }
  void set_poll_interval(uint32_t ms) { this->poll_interval_ms_ = ms; }

  // --- Capteurs DC (jusqu'à 4 canaux selon le modèle décodé depuis le SN) ---
  void set_dc_power_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_power_[ch] = s; }
  void set_dc_current_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_current_[ch] = s; }
  void set_dc_voltage_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_voltage_[ch] = s; }
  void set_dc_energy_today_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_energy_today_[ch] = s; }
  void set_dc_energy_total_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_energy_total_[ch] = s; }
  void set_dc_irradiation_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_irradiation_[ch] = s; }

  // --- Capteurs AC (canal 0 unique, HMS = monophasé) ---
  void set_ac_voltage_sensor(sensor::Sensor *s) { this->ac_voltage_ = s; }
  void set_ac_current_sensor(sensor::Sensor *s) { this->ac_current_ = s; }
  void set_ac_power_sensor(sensor::Sensor *s) { this->ac_power_ = s; }
  void set_ac_frequency_sensor(sensor::Sensor *s) { this->ac_frequency_ = s; }
  void set_ac_power_factor_sensor(sensor::Sensor *s) { this->ac_power_factor_ = s; }
  void set_ac_reactive_power_sensor(sensor::Sensor *s) { this->ac_reactive_power_ = s; }

  // --- Capteurs onduleur (agrégats) ---
  void set_inv_temperature_sensor(sensor::Sensor *s) { this->inv_temperature_ = s; }
  void set_inv_power_sensor(sensor::Sensor *s) { this->inv_power_ = s; }
  void set_inv_energy_today_sensor(sensor::Sensor *s) { this->inv_energy_today_ = s; }
  void set_inv_energy_total_sensor(sensor::Sensor *s) { this->inv_energy_total_ = s; }
  void set_inv_efficiency_sensor(sensor::Sensor *s) { this->inv_efficiency_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
  void set_reachable_sensor(binary_sensor::BinarySensor *s) { this->reachable_sensor_ = s; }
  void set_producing_sensor(binary_sensor::BinarySensor *s) { this->producing_sensor_ = s; }

  uint8_t get_dc_channel_count() const { return this->dc_channel_count_; }

  /// Appelé par la plateforme number : limite de puissance en % (0-100, relatif, non persistant)
  void set_power_limit_percent(float percent);
  /// Limite de puissance en Watts absolus (non persistant)
  void set_power_limit_absolute(float watts);

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  // --- Décodage du numéro de série -> modèle / table d'octets ---
  bool decode_serial_();
  static uint64_t generate_dtu_serial_();

  // --- Init radio spécifique Hoymiles (bancs 860/900MHz, FIFO fusionné, IRQ) ---
  bool init_radio_();
  void switch_to_frequency_(uint32_t freq_hz);
  void switch_to_channel_(uint8_t channel);
  uint32_t frequency_from_channel_(uint8_t channel) const;
  uint8_t channel_from_frequency_(uint32_t freq_hz) const;

  // --- Emission/réception bas niveau (porté de cmt2300wrapper.cpp) ---
  bool cmt_start_tx_(const uint8_t *buf, uint8_t len);
  void process_tx_();
  bool cmt_start_listening_();
  bool cmt_rx_packet_available_();
  uint8_t cmt_read_dynamic_payload_(uint8_t *buf, uint8_t maxlen);

  // --- Construction de trames (porté de commands/*.cpp) ---
  void build_realtime_data_request_(uint8_t *out, uint8_t *out_len);
  void build_request_frame_(uint8_t frame_no, uint8_t *out, uint8_t *out_len);
  void build_active_power_control_(float limit, PowerLimitType type, uint8_t *out, uint8_t *out_len);
  void build_channel_change_(uint8_t channel, uint8_t *out, uint8_t *out_len);

  void send_current_command_();
  void start_command_(PendingCmd cmd, const uint8_t *payload, uint8_t len, uint32_t timeout_ms);

  // --- Réassemblage / vérification des fragments (porté de InverterAbstract) ---
  void clear_rx_fragment_buffer_();
  void add_rx_fragment_(const uint8_t *fragment, uint8_t len);
  uint8_t verify_all_fragments_();  // FRAGMENT_OK(0) / id à retransmettre / codes d'erreur
  bool handle_realtime_response_();
  void apply_statistics_buffer_();
  void publish_sensors_();
  void publish_reachable_();

  // --- Accès aux valeurs décodées (porté de StatisticsParser::getChannelFieldValue) ---
  const byteAssign_t *find_assignment_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const;
  float get_field_value_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const;
  bool has_field_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const;

  cmt2300a::CMT2300AComponent *radio_{nullptr};

  uint64_t inverter_serial_{0};
  uint64_t dtu_serial_{0};
  FrequencyBand frequency_band_{FrequencyBand::EU_860};
  uint32_t poll_interval_ms_{5000};

  uint8_t dc_channel_count_{0};
  std::string type_name_;
  const byteAssign_t *byte_assignment_{nullptr};
  uint8_t byte_assignment_size_{0};
  uint8_t expected_byte_count_{0};

  uint32_t work_frequency_{0};
  uint8_t work_channel_{0};

  uint8_t stats_buf_[STATISTIC_PACKET_SIZE]{};
  bool has_valid_stats_{false};

  // Réassemblage des fragments de la réponse en cours
  fragment_t rx_fragments_[MAX_RF_FRAGMENT_COUNT]{};
  uint8_t rx_fragment_last_id_{0};
  uint8_t rx_fragment_max_id_{0};
  uint8_t rx_retransmit_count_{0};

  // Commande actuellement en vol
  PendingCmd pending_cmd_{CMD_NONE};
  RadioOpState op_state_{OP_IDLE};
  uint8_t tx_payload_[32]{};
  uint8_t tx_payload_len_{0};
  uint8_t send_count_{0};
  uint32_t cmd_deadline_{0};

  // Emission Tx non bloquante : le chip transmet en arrière-plan, process_tx_()
  // (appelée à chaque tick de loop()) vérifie TX_DONE sans jamais attendre activement.
  bool tx_sending_{false};
  uint32_t tx_start_{0};
  bool tx_restore_channel_{false};
  uint8_t tx_restore_channel_value_{0};
  bool tx_release_lock_after_{false};

  // Fiabilité de la liaison (pour déclencher un ChannelChangeCommand)
  uint32_t rx_failure_count_{0};
  static const uint8_t REACHABLE_THRESHOLD = 3;

  // Réglage de puissance en attente (envoyé dès que le canal radio est libre)
  bool power_limit_pending_{false};
  float power_limit_value_{100.0f};
  PowerLimitType power_limit_type_{POWER_RELATIVE};

  uint32_t last_poll_{0};
  int8_t last_rssi_dbm_{-127};

  sensor::Sensor *dc_power_[4]{};
  sensor::Sensor *dc_current_[4]{};
  sensor::Sensor *dc_voltage_[4]{};
  sensor::Sensor *dc_energy_today_[4]{};
  sensor::Sensor *dc_energy_total_[4]{};
  sensor::Sensor *dc_irradiation_[4]{};

  sensor::Sensor *ac_voltage_{nullptr};
  sensor::Sensor *ac_current_{nullptr};
  sensor::Sensor *ac_power_{nullptr};
  sensor::Sensor *ac_frequency_{nullptr};
  sensor::Sensor *ac_power_factor_{nullptr};
  sensor::Sensor *ac_reactive_power_{nullptr};

  sensor::Sensor *inv_temperature_{nullptr};
  sensor::Sensor *inv_power_{nullptr};
  sensor::Sensor *inv_energy_today_{nullptr};
  sensor::Sensor *inv_energy_total_{nullptr};
  sensor::Sensor *inv_efficiency_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
  binary_sensor::BinarySensor *reachable_sensor_{nullptr};
  binary_sensor::BinarySensor *producing_sensor_{nullptr};
};

}  // namespace hms
}  // namespace esphome
