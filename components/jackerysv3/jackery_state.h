#pragma once

// Decoding of the Jackery SolarVault 3 MQTT protocol and the derived (calculated) values.
//
// Faithful C++ port of the data handling of the official Home Assistant integration
// (Jackery-Official/jackery, custom_components/jackery/sensor.py): the same message types, the same field
// aliases, the same merge rules and the same "energy flow" formulas. This file has no ESPHome dependency
// (only ArduinoJson) so it can be unit tested on a host machine.

#include <ArduinoJson.h>

#include <cstdint>
#include <string>
#include <vector>

namespace esphome::jackerysv3 {

// ---------------------------------------------------------------------------------------------------------------
// Entity kinds. The lower-case names below (see the tables in jackery_state.cpp) are the keys passed by the
// Python code generation; `index` selects the PV channel (0..3), the plug (0..9) or the CT (0).
// ---------------------------------------------------------------------------------------------------------------

enum class SensorKind : uint8_t {
  // flat device level
  STATUS_CODE,
  WORK_MODE_CODE,
  SOLAR_POWER,
  SOLAR_ENERGY,
  PV_TO_BATTERY_ENERGY,
  PV_TO_SOCKET_ENERGY,
  PV_TO_GRID_ENERGY,
  GRID_TO_SOCKET_ENERGY,
  GRID_TO_BATTERY_ENERGY,
  BATTERY_TO_SOCKET_ENERGY,
  BATTERY_TO_GRID_ENERGY,
  SOCKET_TO_BATTERY_ENERGY,
  SOCKET_TO_GRID_ENERGY,
  // dc_channels (index 0..3)
  PV_POWER,
  PV_ENERGY,
  // ac
  AC_GRID_INPUT_POWER,
  AC_GRID_OUTPUT_POWER,
  AC_GRID_INPUT_ENERGY,
  AC_GRID_OUTPUT_ENERGY,
  AC_GRID_POWER,
  AC_HOME_POWER,
  AC_OTHER_LOAD_POWER,
  AC_MAX_FEED_IN_POWER,
  AC_SOCKET_POWER,
  AC_SOCKET_INPUT_POWER,
  AC_SOCKET_INPUT_ENERGY,
  AC_SOCKET_OUTPUT_ENERGY,
  // battery
  BATTERY_SOC,
  BATTERY_AVERAGE_SOC,
  BATTERY_TEMPERATURE,
  BATTERY_PACK_COUNT,
  BATTERY_CHARGE_POWER,
  BATTERY_DISCHARGE_POWER,
  BATTERY_NET_POWER,
  BATTERY_CHARGE_ENERGY,
  BATTERY_DISCHARGE_ENERGY,
  // ct (index 0)
  CT_FORWARD_POWER,
  CT_REVERSE_POWER,
  CT_FORWARD_ENERGY,
  CT_REVERSE_ENERGY,
  CT_PHASE_A_FORWARD_POWER,
  CT_PHASE_B_FORWARD_POWER,
  CT_PHASE_C_FORWARD_POWER,
  CT_PHASE_A_REVERSE_POWER,
  CT_PHASE_B_REVERSE_POWER,
  CT_PHASE_C_REVERSE_POWER,
  // plugs (index 0..9)
  PLUG_POWER,
  PLUG_ENERGY,
};

enum class BinaryKind : uint8_t { ON_GRID, CT_ONLINE, GRID_METER_LINK, SOCKET_OK };
enum class TextKind : uint8_t { STATUS, WORK_MODE, FIRMWARE_VERSION, MODEL, CT_TYPE };
enum class SwitchKind : uint8_t { AC_SOCKET, AUTO_STANDBY_ALLOWED, PLUG };
enum class NumberKind : uint8_t { SOC_CHARGE_LIMIT, SOC_DISCHARGE_LIMIT, MAX_OUTPUT_POWER };
enum class SelectKind : uint8_t { AUTO_STANDBY_MODE };

bool parse_sensor_kind(const char *key, SensorKind &out);
bool parse_binary_kind(const char *key, BinaryKind &out);
bool parse_text_kind(const char *key, TextKind &out);
bool parse_switch_kind(const char *key, SwitchKind &out);
bool parse_number_kind(const char *key, NumberKind &out);
bool parse_select_kind(const char *key, SelectKind &out);

/// JSON field written by a number / select / switch (`{"cmd":5,"rc":1,"<field>":value}`)
const char *number_json_field(NumberKind kind);
const char *switch_json_field(SwitchKind kind);
const char *select_json_field(SelectKind kind);

static constexpr uint8_t MAX_PV_CHANNELS = 4;
static constexpr uint8_t MAX_PLUGS = 10;

/// Optional number (`has` false: the field was never reported)
struct Opt {
  double v{0.0};
  bool has{false};
  void set(double x) {
    this->v = x;
    this->has = true;
  }
  double or0() const { return this->has ? this->v : 0.0; }
};

/// Smart plug (devType 6) as reported by the host
struct PlugState {
  std::string sn;
  int dev_type{6};
  Opt out_pw, power_alias, in_pw, total_egy;  // `power` is the HA integration's fallback name of `outPw`
  Opt switch_sta, sys_switch;                 // on/off state: switchSta, else sysSwitch
  int comm_mode{-1};                          // 1 local, 2 cloud, -1 unknown
  int comm_state{-1};
  uint32_t last_seen_ms{0};
  bool seen{false};

  /// Output power: `outPw`, else `power`
  const Opt &power() const { return this->out_pw.has ? this->out_pw : this->power_alias; }
  /// On/off state: `switchSta`, else `sysSwitch`
  const Opt &switch_state() const { return this->switch_sta.has ? this->switch_sta : this->sys_switch; }
};

/// CT / smart meter (devType 2, 3 or 4)
struct CtState {
  std::string sn;
  int dev_type{3};
  Opt t_pw, tn_pw, a_pw, b_pw, c_pw, an_pw, bn_pw, cn_pw;
  Opt t_egy, tn_egy;
  int sub_type{-1};
  int comm_state{-1};
  uint32_t last_seen_ms{0};
  bool seen{false};
};

/// Values derived from the raw fields (HA integration `_calculate_energy_flow`)
struct Calc {
  bool valid{false};
  double ac_socket_power{0};
  double home_power{0};
  double batt_net_power{0};
  double batt_charge_power{0};
  double batt_discharge_power{0};
  double grid_net_power{0};
  bool grid_available{false};
};

class JackeryState {
 public:
  explicit JackeryState(std::string device_sn) : device_sn_(std::move(device_sn)) {}

  struct Result {
    bool main_updated{false};
    bool sub_updated{false};
    bool token_error{false};  // type 123 / errorCode 401
    bool recognized{false};   // the message carried data we understand
  };

  /// Merge one JSON message (the root object) into the state.
  Result ingest(JsonObjectConst root, uint32_t now_ms);

  /// Recompute the derived values. Called by ingest().
  void recalc();

  // ---- values (return false when there is nothing to publish yet) -----------------------------------------
  bool sensor_value(SensorKind kind, uint8_t index, float &out) const;
  bool binary_value(BinaryKind kind, bool &out) const;
  bool text_value(TextKind kind, uint8_t index, std::string &out) const;
  bool switch_value(SwitchKind kind, uint8_t index, bool &out) const;
  bool number_value(NumberKind kind, float &out) const;
  bool select_index(SelectKind kind, size_t &out) const;
  /// Bounds currently reported by the device for a number (minSocChg, maxSocChg ...), when known
  bool number_bounds(NumberKind kind, float &min_v, float &max_v) const;

  // ---- plugs ----------------------------------------------------------------------------------------------
  /// Pin plug slot `index` to a serial number (otherwise slots are filled in discovery order)
  void set_plug_sn(uint8_t index, const std::string &sn);
  /// Plug bound to slot `index` (nullptr until it has been discovered)
  const PlugState *plug(uint8_t index) const;
  const CtState *ct() const { return this->cts_.empty() ? nullptr : &this->cts_[0]; }
  /// 1 local, 2 cloud, -1 unknown
  int plug_comm_mode(uint8_t index) const;

  // ---- availability ---------------------------------------------------------------------------------------
  bool main_received() const { return this->main_received_; }
  /// Per-device freshness (plug / CT): false once not seen for `timeout_ms`
  bool plug_fresh(uint8_t index, uint32_t now_ms, uint32_t timeout_ms) const;
  bool ct_fresh(uint32_t now_ms, uint32_t timeout_ms) const;

  const std::string &device_type_name() const { return this->model_name_; }
  const Calc &calc() const { return this->calc_; }

  /// Raw field access for tests / diagnostics (JSON field name)
  bool raw(const char *field, double &out) const;

 protected:
  // Raw fields of the host device (JSON names in jackery_state.cpp, same order)
  enum Field : uint8_t {
    F_STAT,
    F_WORK_MODE,
    F_ONGRID_STAT,
    F_CT_STAT,
    F_GRID_SATE,
    F_OTHER_LOAD_PW,
    F_MAX_FEED_GRID,
    F_BAT_SOC,
    F_SOC,
    F_BAT_IN_PW,
    F_BAT_OUT_PW,
    F_CELL_TEMP,
    F_BAT_NUM,
    F_BAT_CHG_EGY,
    F_BAT_DISCHG_EGY,
    F_PV_PW,
    F_PV_EGY,
    F_PV1,
    F_PV2,
    F_PV3,
    F_PV4,
    F_PV1_EGY,
    F_PV2_EGY,
    F_PV3_EGY,
    F_PV4_EGY,
    F_GRID_IN_PW,
    F_IN_ONGRID_EGY,
    F_GRID_OUT_PW,
    F_OUT_ONGRID_EGY,
    F_MAX_OUT_PW,
    F_OUT_EPS_EGY,
    F_SW_EPS_IN_PW,
    F_SW_EPS_OUT_PW,
    F_IN_EPS_EGY,
    F_SW_EPS_STATE,
    F_SW_EPS,
    F_SOC_CHG_LIMIT,
    F_SOC_DISCHG_LIMIT,
    F_IS_AUTO_STANDBY,
    F_AUTO_STANDBY,
    F_MIN_SOC_CHG,
    F_MAX_SOC_CHG,
    F_MIN_SOC_DISCHG,
    F_MAX_SOC_DISCHG,
    F_IN_ONGRID_PW,
    F_OUT_ONGRID_PW,
    F_IN_GRID_SIDE_PW,
    F_OUT_GRID_SIDE_PW,
    F_AC_OT_BAT_EGY,
    F_PV_OT_BAT_EGY,
    F_PV_OT_AC_EGY,
    F_PV_OT_ONGRID_EGY,
    F_ONGRID_OT_AC_LOAD_EGY,
    F_BAT_OT_AC_EGY,
    F_BAT_OT_GRID_EGY,
    F_ONGRID_OT_BAT_EGY,
    F_AC_OT_ONGRID_EGY,
    F_COUNT
  };

  // ingestion helpers
  bool merge_main_(JsonObjectConst body, bool skip_sub_keys);
  bool merge_arrays_(JsonObjectConst body, uint32_t now_ms);
  bool merge_point_(JsonObjectConst body, uint32_t now_ms);
  void apply_101_(JsonObjectConst body, uint32_t now_ms);
  void capture_meta_(JsonObjectConst root, JsonVariantConst body);
  PlugState *find_plug_(const std::string &sn);
  CtState *find_ct_(const std::string &sn);
  PlugState *upsert_plug_(const std::string &sn);
  CtState *upsert_ct_(const std::string &sn);
  static void fill_plug_(PlugState &p, JsonObjectConst item);
  static void fill_ct_(CtState &c, JsonObjectConst item);
  void touch_sub_(const std::string &sn, uint32_t now_ms);
  const PlugState *slot_plug_(uint8_t index) const;

  std::string device_sn_;
  std::string soft_ver_;
  std::string model_name_;
  int device_type_{-1};

  // main device raw fields (indexed by Field)
  std::vector<Opt> f_;

  std::vector<PlugState> plugs_;
  std::vector<CtState> cts_;
  std::string slot_sn_[MAX_PLUGS];
  bool main_received_{false};
  Calc calc_;
};

/// Decimal helper shared with the hub: JSON number / numeric string / bool -> double
bool json_to_double(JsonVariantConst v, double &out);

}  // namespace esphome::jackerysv3
