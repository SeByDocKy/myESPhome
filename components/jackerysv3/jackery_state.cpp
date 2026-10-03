#include "jackery_state.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace esphome::jackerysv3 {

// ---------------------------------------------------------------------------------------------------------------
// Entity key tables (the strings are the ones used by the Python code generation)
// ---------------------------------------------------------------------------------------------------------------

namespace {

template<typename K> struct KindName {
  const char *key;
  K kind;
};

const KindName<SensorKind> SENSOR_KINDS[] = {
    {"status_code", SensorKind::STATUS_CODE},
    {"work_mode_code", SensorKind::WORK_MODE_CODE},
    {"solar_power", SensorKind::SOLAR_POWER},
    {"solar_energy", SensorKind::SOLAR_ENERGY},
    {"pv_to_battery_energy", SensorKind::PV_TO_BATTERY_ENERGY},
    {"pv_to_socket_energy", SensorKind::PV_TO_SOCKET_ENERGY},
    {"pv_to_grid_energy", SensorKind::PV_TO_GRID_ENERGY},
    {"grid_to_socket_energy", SensorKind::GRID_TO_SOCKET_ENERGY},
    {"grid_to_battery_energy", SensorKind::GRID_TO_BATTERY_ENERGY},
    {"battery_to_socket_energy", SensorKind::BATTERY_TO_SOCKET_ENERGY},
    {"battery_to_grid_energy", SensorKind::BATTERY_TO_GRID_ENERGY},
    {"socket_to_battery_energy", SensorKind::SOCKET_TO_BATTERY_ENERGY},
    {"socket_to_grid_energy", SensorKind::SOCKET_TO_GRID_ENERGY},
    {"pv_power", SensorKind::PV_POWER},
    {"pv_energy", SensorKind::PV_ENERGY},
    {"ac_grid_input_power", SensorKind::AC_GRID_INPUT_POWER},
    {"ac_grid_output_power", SensorKind::AC_GRID_OUTPUT_POWER},
    {"ac_grid_input_energy", SensorKind::AC_GRID_INPUT_ENERGY},
    {"ac_grid_output_energy", SensorKind::AC_GRID_OUTPUT_ENERGY},
    {"ac_grid_power", SensorKind::AC_GRID_POWER},
    {"ac_home_power", SensorKind::AC_HOME_POWER},
    {"ac_other_load_power", SensorKind::AC_OTHER_LOAD_POWER},
    {"ac_max_feed_in_power", SensorKind::AC_MAX_FEED_IN_POWER},
    {"ac_socket_power", SensorKind::AC_SOCKET_POWER},
    {"ac_socket_input_power", SensorKind::AC_SOCKET_INPUT_POWER},
    {"ac_socket_input_energy", SensorKind::AC_SOCKET_INPUT_ENERGY},
    {"ac_socket_output_energy", SensorKind::AC_SOCKET_OUTPUT_ENERGY},
    {"battery_soc", SensorKind::BATTERY_SOC},
    {"battery_average_soc", SensorKind::BATTERY_AVERAGE_SOC},
    {"battery_temperature", SensorKind::BATTERY_TEMPERATURE},
    {"battery_pack_count", SensorKind::BATTERY_PACK_COUNT},
    {"battery_charge_power", SensorKind::BATTERY_CHARGE_POWER},
    {"battery_discharge_power", SensorKind::BATTERY_DISCHARGE_POWER},
    {"battery_net_power", SensorKind::BATTERY_NET_POWER},
    {"battery_charge_energy", SensorKind::BATTERY_CHARGE_ENERGY},
    {"battery_discharge_energy", SensorKind::BATTERY_DISCHARGE_ENERGY},
    {"ct_forward_power", SensorKind::CT_FORWARD_POWER},
    {"ct_reverse_power", SensorKind::CT_REVERSE_POWER},
    {"ct_forward_energy", SensorKind::CT_FORWARD_ENERGY},
    {"ct_reverse_energy", SensorKind::CT_REVERSE_ENERGY},
    {"ct_phase_a_forward_power", SensorKind::CT_PHASE_A_FORWARD_POWER},
    {"ct_phase_b_forward_power", SensorKind::CT_PHASE_B_FORWARD_POWER},
    {"ct_phase_c_forward_power", SensorKind::CT_PHASE_C_FORWARD_POWER},
    {"ct_phase_a_reverse_power", SensorKind::CT_PHASE_A_REVERSE_POWER},
    {"ct_phase_b_reverse_power", SensorKind::CT_PHASE_B_REVERSE_POWER},
    {"ct_phase_c_reverse_power", SensorKind::CT_PHASE_C_REVERSE_POWER},
    {"plug_power", SensorKind::PLUG_POWER},
    {"plug_energy", SensorKind::PLUG_ENERGY},
};

const KindName<BinaryKind> BINARY_KINDS[] = {
    {"on_grid", BinaryKind::ON_GRID},
    {"ct_online", BinaryKind::CT_ONLINE},
    {"grid_meter_link", BinaryKind::GRID_METER_LINK},
    {"socket_ok", BinaryKind::SOCKET_OK},
};

const KindName<TextKind> TEXT_KINDS[] = {
    {"status", TextKind::STATUS},
    {"work_mode", TextKind::WORK_MODE},
    {"firmware_version", TextKind::FIRMWARE_VERSION},
    {"model", TextKind::MODEL},
    {"ct_type", TextKind::CT_TYPE},
};

const KindName<SwitchKind> SWITCH_KINDS[] = {
    {"ac_socket", SwitchKind::AC_SOCKET},
    {"auto_standby_allowed", SwitchKind::AUTO_STANDBY_ALLOWED},
    {"plug", SwitchKind::PLUG},
};

const KindName<NumberKind> NUMBER_KINDS[] = {
    {"soc_charge_limit", NumberKind::SOC_CHARGE_LIMIT},
    {"soc_discharge_limit", NumberKind::SOC_DISCHARGE_LIMIT},
    {"max_output_power", NumberKind::MAX_OUTPUT_POWER},
};

const KindName<SelectKind> SELECT_KINDS[] = {
    {"auto_standby_mode", SelectKind::AUTO_STANDBY_MODE},
};

template<typename K, size_t N> bool lookup(const KindName<K> (&table)[N], const char *key, K &out) {
  if (key == nullptr)
    return false;
  for (const auto &e : table) {
    if (strcmp(e.key, key) == 0) {
      out = e.kind;
      return true;
    }
  }
  return false;
}

// JSON field names of the main device, in the order of JackeryState::Field
const char *const FIELD_NAMES[] = {
    "stat",           "workMode",         "ongridStat",      "ctStat",        "gridSate",
    "otherLoadPw",    "maxFeedGrid",      "batSoc",          "soc",           "batInPw",
    "batOutPw",       "cellTemp",         "batNum",          "batChgEgy",     "batDisChgEgy",
    "pvPw",           "pvEgy",            "pv1",             "pv2",           "pv3",
    "pv4",            "pv1Egy",           "pv2Egy",          "pv3Egy",        "pv4Egy",
    "gridInPw",       "inOngridEgy",      "gridOutPw",       "outOngridEgy",  "maxOutPw",
    "outEpsEgy",      "swEpsInPw",        "swEpsOutPw",      "inEpsEgy",      "swEpsState",
    "swEps",          "socChgLimit",      "socDischgLimit",  "isAutoStandby", "autoStandby",
    "minSocChg",      "maxSocChg",        "minSocDischg",    "maxSocDischg",  "inOngridPw",
    "outOngridPw",    "inGridSidePw",     "outGridSidePw",   "acOtBatEgy",    "pvOtBatEgy",
    "pvOtAcEgy",      "pvOtOngridEgy",    "ongridOtAcLoadEgy", "batOtAcEgy",  "batOtGridEgy",
    "ongridOtBatEgy", "acOtOngridEgy",
};
static_assert(sizeof(FIELD_NAMES) / sizeof(FIELD_NAMES[0]) == 57, "FIELD_NAMES must match JackeryState::Field");

// Keys that mark a payload without type / body wrapper as a "flat status message"
const char *const FLAT_PAYLOAD_KEYS[] = {"batSoc",     "soc",          "pvPw",        "stat",          "workMode",
                                         "inOngridPw", "outOngridPw",  "gridInPw",    "gridOutPw",     "inGridSidePw",
                                         "outGridSidePw", "swEpsInPw", "swEpsOutPw",  "batInPw",       "batOutPw",
                                         "otherLoadPw"};

// Keys of a body that are not main device fields (HA `main_body` filter)
const char *const SUB_KEYS[] = {"plugs", "plug", "socket", "sockets", "cts", "ct", "deviceSn", "sn"};

const char *const DEVICE_STATUS_NAMES[] = {"Normal", "Waiting", "Alarm", "Fault", "Standby", "Low power"};
const char *const WORK_MODE_NAMES[] = {"Invalid",           "Disable energy scheduling", "Self-consumption",
                                       "Battery priority", "User-defined",              "TOU",
                                       "Feed-in priority", "Dynamic pricing"};
const char *const CT_SUBTYPE_NAMES[] = {"Shelly Single Phase",
                                        "Shelly Three Phase",
                                        "Shelly 63A",
                                        "Eastron Single Phase (4002)",
                                        "Eastron Three Phase (4003)",
                                        "Jackery Wireless Smart Meter (US L1/L2 4007)",
                                        "Jackery Smart Meter 3P (UK 4008)"};

bool is_flat_key(const char *k) {
  for (const char *f : FLAT_PAYLOAD_KEYS)
    if (strcmp(k, f) == 0)
      return true;
  return false;
}

bool is_sub_key(const char *k) {
  for (const char *f : SUB_KEYS)
    if (strcmp(k, f) == 0)
      return true;
  return false;
}

bool is_ct_dev_type(int t) { return t == 2 || t == 3 || t == 4; }

bool has_nn(JsonObjectConst o, const char *key) { return !o[key].isNull(); }

std::string str_field(JsonObjectConst o, const char *key) {
  JsonVariantConst v = o[key];
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    return s != nullptr ? std::string(s) : std::string();
  }
  return std::string();
}

/// deviceSn, else sn (HA `_subdevice_sn`: `item.get("deviceSn") or item.get("sn")`)
std::string sub_sn(JsonObjectConst o) {
  std::string s = str_field(o, "deviceSn");
  if (s.empty())
    s = str_field(o, "sn");
  return s;
}

bool to_int(JsonVariantConst v, int &out) {
  double d;
  if (!json_to_double(v, d))
    return false;
  out = static_cast<int>(d);
  return true;
}

/// Set `o` from the first non-null of `a`, `b` (HA: `x.get(A) if x.get(A) is not None else x.get(B)`)
void set_opt2(Opt &o, JsonObjectConst item, const char *a, const char *b) {
  double d;
  if (json_to_double(item[a], d)) {
    o.set(d);
    return;
  }
  if (b != nullptr && json_to_double(item[b], d))
    o.set(d);
}

double sf(const Opt &o) { return o.has ? o.v : 0.0; }

/// HA `_pick_best_power_net`: the non-zero candidate of largest magnitude, else the last one
double pick_best(const std::vector<double> &c) {
  if (c.empty())
    return 0.0;
  bool found = false;
  double best = 0.0;
  for (double v : c) {
    if (std::fabs(v) > 0 && (!found || std::fabs(v) > std::fabs(best))) {
      best = v;
      found = true;
    }
  }
  return found ? best : c.back();
}

/// HA `_extract_ct_grid_power`: (grid_buy, grid_sell, has_power_fields)
void ct_totals(const CtState &c, double &buy, double &sell, bool &has) {
  Opt t = c.t_pw;
  if (!t.has || t.v == 0) {
    if (c.a_pw.has || c.b_pw.has || c.c_pw.has)
      t.set(sf(c.a_pw) + sf(c.b_pw) + sf(c.c_pw));
  }
  Opt tn = c.tn_pw;
  if (!tn.has || tn.v == 0) {
    if (c.an_pw.has || c.bn_pw.has || c.cn_pw.has)
      tn.set(sf(c.an_pw) + sf(c.bn_pw) + sf(c.cn_pw));
  }
  has = t.has || tn.has;
  buy = has ? sf(t) : 0.0;
  sell = has ? sf(tn) : 0.0;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------------------------------------------

bool parse_sensor_kind(const char *key, SensorKind &out) { return lookup(SENSOR_KINDS, key, out); }
bool parse_binary_kind(const char *key, BinaryKind &out) { return lookup(BINARY_KINDS, key, out); }
bool parse_text_kind(const char *key, TextKind &out) { return lookup(TEXT_KINDS, key, out); }
bool parse_switch_kind(const char *key, SwitchKind &out) { return lookup(SWITCH_KINDS, key, out); }
bool parse_number_kind(const char *key, NumberKind &out) { return lookup(NUMBER_KINDS, key, out); }
bool parse_select_kind(const char *key, SelectKind &out) { return lookup(SELECT_KINDS, key, out); }

const char *number_json_field(NumberKind kind) {
  switch (kind) {
    case NumberKind::SOC_CHARGE_LIMIT:
      return "socChgLimit";
    case NumberKind::SOC_DISCHARGE_LIMIT:
      return "socDischgLimit";
    case NumberKind::MAX_OUTPUT_POWER:
      return "maxOutPw";
  }
  return "";
}

const char *switch_json_field(SwitchKind kind) {
  switch (kind) {
    case SwitchKind::AC_SOCKET:
      return "swEps";
    case SwitchKind::AUTO_STANDBY_ALLOWED:
      return "isAutoStandby";
    case SwitchKind::PLUG:
      return "";
  }
  return "";
}

const char *select_json_field(SelectKind kind) {
  (void) kind;
  return "autoStandby";
}

bool json_to_double(JsonVariantConst v, double &out) {
  if (v.isNull())
    return false;
  if (v.is<bool>()) {
    out = v.as<bool>() ? 1.0 : 0.0;
    return true;
  }
  if (v.is<double>()) {
    out = v.as<double>();
    return true;
  }
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    if (s == nullptr || *s == '\0')
      return false;
    char *end = nullptr;
    double d = strtod(s, &end);
    if (end != s && *end == '\0') {
      out = d;
      return true;
    }
    return false;
  }
  if (v.is<JsonObjectConst>()) {
    // HA integration: PV power may arrive as {"pvPw": x} / {"w": x} / {"power": x}
    JsonObjectConst o = v.as<JsonObjectConst>();
    for (const char *k : {"pvPw", "w", "power"}) {
      double d;
      if (json_to_double(o[k], d)) {
        out = d;
        return true;
      }
    }
  }
  return false;
}

// ---------------------------------------------------------------------------------------------------------------
// Ingestion
// ---------------------------------------------------------------------------------------------------------------

bool JackeryState::raw(const char *field, double &out) const {
  for (size_t i = 0; i < F_COUNT; i++) {
    if (strcmp(FIELD_NAMES[i], field) == 0 && i < this->f_.size() && this->f_[i].has) {
      out = this->f_[i].v;
      return true;
    }
  }
  return false;
}

bool JackeryState::merge_main_(JsonObjectConst body, bool /*skip_sub_keys*/) {
  if (this->f_.size() != F_COUNT)
    this->f_.assign(F_COUNT, Opt{});
  bool any = false;
  for (JsonPairConst kv : body) {
    const char *k = kv.key().c_str();
    for (size_t i = 0; i < F_COUNT; i++) {
      if (strcmp(FIELD_NAMES[i], k) == 0) {
        double d;
        if (json_to_double(kv.value(), d)) {
          this->f_[i].set(d);
          any = true;
        }
        break;
      }
    }
  }
  // Aliases (HA `_normalize_payload_fields`): only used when the canonical field is absent in the message
  double d;
  if (!has_nn(body, "gridInPw") && json_to_double(body["gridBuyPw"], d)) {
    this->f_[F_GRID_IN_PW].set(d);
    any = true;
  }
  if (!has_nn(body, "gridOutPw") && json_to_double(body["gridSellPw"], d)) {
    this->f_[F_GRID_OUT_PW].set(d);
    any = true;
  }
  if (!has_nn(body, "workMode") && json_to_double(body["workModel"], d)) {
    this->f_[F_WORK_MODE].set(d);
    any = true;
  }
  return any;
}

void JackeryState::capture_meta_(JsonObjectConst root, JsonVariantConst body) {
  int t;
  if (to_int(root["deviceType"], t) && t != this->device_type_) {
    this->device_type_ = t;
    switch (t) {
      case 1:
        this->model_name_ = "Battery Pack";
        break;
      case 2:
        this->model_name_ = "CT/Meter Collector/Meter";
        break;
      case 3:
        this->model_name_ = "DIY3";
        break;
      case 4:
        this->model_name_ = "Meter Collector";
        break;
      default:
        this->model_name_ = "Energy Monitor";
        break;
    }
  }
  JsonVariantConst sv = root["softver"];
  if (sv.isNull() && body.is<JsonObjectConst>())
    sv = body.as<JsonObjectConst>()["softver"];
  if (!sv.isNull()) {
    std::string s;
    if (sv.is<const char *>()) {
      const char *c = sv.as<const char *>();
      s = c != nullptr ? c : "";
    } else {
      serializeJson(sv, s);
    }
    this->soft_ver_ = s;
  }
}

PlugState *JackeryState::find_plug_(const std::string &sn) {
  for (auto &p : this->plugs_)
    if (p.sn == sn)
      return &p;
  return nullptr;
}

CtState *JackeryState::find_ct_(const std::string &sn) {
  for (auto &c : this->cts_)
    if (c.sn == sn)
      return &c;
  return nullptr;
}

PlugState *JackeryState::upsert_plug_(const std::string &sn) {
  PlugState *p = this->find_plug_(sn);
  if (p != nullptr)
    return p;
  if (this->plugs_.size() >= 16)  // safety bound (the host supports up to 10 plugs)
    return nullptr;
  this->plugs_.emplace_back();
  this->plugs_.back().sn = sn;
  return &this->plugs_.back();
}

CtState *JackeryState::upsert_ct_(const std::string &sn) {
  CtState *c = this->find_ct_(sn);
  if (c != nullptr)
    return c;
  if (this->cts_.size() >= 4)
    return nullptr;
  this->cts_.emplace_back();
  this->cts_.back().sn = sn;
  return &this->cts_.back();
}

void JackeryState::fill_plug_(PlugState &p, JsonObjectConst item) {
  int i;
  double d;
  if (to_int(item["devType"], i))
    p.dev_type = i;
  if (json_to_double(item["outPw"], d))
    p.out_pw.set(d);
  if (json_to_double(item["power"], d))
    p.power_alias.set(d);
  if (json_to_double(item["inPw"], d))
    p.in_pw.set(d);
  if (json_to_double(item["totalEgy"], d))
    p.total_egy.set(d);
  if (json_to_double(item["switchSta"], d))
    p.switch_sta.set(d);
  if (json_to_double(item["sysSwitch"], d))
    p.sys_switch.set(d);
  if (to_int(item["commMode"], i))
    p.comm_mode = i;
  if (to_int(item["commState"], i))
    p.comm_state = i;
}

void JackeryState::fill_ct_(CtState &c, JsonObjectConst item) {
  int i;
  if (to_int(item["devType"], i))
    c.dev_type = i;
  // The reference integration reads the capitalised key first and falls back to the lower-case one
  set_opt2(c.t_pw, item, "TphasePw", "tPhasePw");
  set_opt2(c.tn_pw, item, "TnphasePw", "tnPhasePw");
  set_opt2(c.a_pw, item, "AphasePw", "aPhasePw");
  set_opt2(c.b_pw, item, "BphasePw", "bPhasePw");
  set_opt2(c.c_pw, item, "CphasePw", "cPhasePw");
  set_opt2(c.an_pw, item, "AnphasePw", "anPhasePw");
  set_opt2(c.bn_pw, item, "BnphasePw", "bnPhasePw");
  set_opt2(c.cn_pw, item, "CnphasePw", "cnPhasePw");
  set_opt2(c.t_egy, item, "TphaseEgy", "tPhaseEgy");
  set_opt2(c.tn_egy, item, "TnphaseEgy", "tnPhaseEgy");
  if (to_int(item["subType"], i))
    c.sub_type = i;
  if (to_int(item["commState"], i))
    c.comm_state = i;
}

void JackeryState::touch_sub_(const std::string &sn, uint32_t now_ms) {
  if (sn.empty() || sn == this->device_sn_ || sn == "system")
    return;
  if (PlugState *p = this->find_plug_(sn)) {
    p->last_seen_ms = now_ms;
    p->seen = true;
  }
  if (CtState *c = this->find_ct_(sn)) {
    c->last_seen_ms = now_ms;
    c->seen = true;
  }
}

bool JackeryState::merge_arrays_(JsonObjectConst body, uint32_t now_ms) {
  bool updated = false;

  // plugs: first non-empty array among plug / plugs / socket / sockets
  JsonArrayConst raw_plugs;
  for (const char *k : {"plug", "plugs", "socket", "sockets"}) {
    JsonVariantConst v = body[k];
    if (v.is<JsonArrayConst>() && v.as<JsonArrayConst>().size() > 0) {
      raw_plugs = v.as<JsonArrayConst>();
      break;
    }
  }
  if (!raw_plugs.isNull()) {
    for (JsonVariantConst iv : raw_plugs) {
      if (!iv.is<JsonObjectConst>())
        continue;
      JsonObjectConst item = iv.as<JsonObjectConst>();
      std::string sn = sub_sn(item);
      if (sn.empty() || sn == this->device_sn_)
        continue;
      int dt = 6;
      to_int(item["devType"], dt);
      if (is_ct_dev_type(dt)) {
        if (CtState *c = this->upsert_ct_(sn)) {
          fill_ct_(*c, item);
          c->last_seen_ms = now_ms;
          c->seen = true;
        }
      } else if (PlugState *p = this->upsert_plug_(sn)) {
        p->dev_type = dt;
        fill_plug_(*p, item);
        p->last_seen_ms = now_ms;
        p->seen = true;
      }
    }
    updated = true;
  }

  JsonArrayConst raw_cts;
  for (const char *k : {"ct", "cts"}) {
    JsonVariantConst v = body[k];
    if (v.is<JsonArrayConst>() && v.as<JsonArrayConst>().size() > 0) {
      raw_cts = v.as<JsonArrayConst>();
      break;
    }
  }
  if (!raw_cts.isNull()) {
    for (JsonVariantConst iv : raw_cts) {
      if (!iv.is<JsonObjectConst>())
        continue;
      JsonObjectConst item = iv.as<JsonObjectConst>();
      std::string sn = sub_sn(item);
      if (sn.empty() || sn == this->device_sn_)
        continue;
      if (CtState *c = this->upsert_ct_(sn)) {
        fill_ct_(*c, item);
        c->last_seen_ms = now_ms;
        c->seen = true;
      }
    }
    updated = true;
  }
  return updated;
}

bool JackeryState::merge_point_(JsonObjectConst body, uint32_t now_ms) {
  std::string sn = sub_sn(body);
  if (sn.empty() || sn == this->device_sn_ || sn == "system")
    return false;

  if (PlugState *p = this->find_plug_(sn)) {
    fill_plug_(*p, body);
    p->last_seen_ms = now_ms;
    p->seen = true;
    return true;
  }
  if (CtState *c = this->find_ct_(sn)) {
    fill_ct_(*c, body);
    c->last_seen_ms = now_ms;
    c->seen = true;
    return true;
  }

  int dev_type = -1;
  bool have = to_int(body["devType"], dev_type);
  if (!have) {
    for (const char *k : {"switchSta", "sysSwitch", "outPw", "inPw", "totalEgy"}) {
      if (has_nn(body, k)) {
        dev_type = 6;
        have = true;
        break;
      }
    }
  }
  if (!have) {
    for (const char *k : {"AphasePw", "aPhasePw", "phasePw", "subType"}) {
      if (has_nn(body, k)) {
        dev_type = 3;
        have = true;
        break;
      }
    }
  }
  if (have && dev_type == 6) {
    if (PlugState *p = this->upsert_plug_(sn)) {
      fill_plug_(*p, body);
      p->dev_type = 6;
      p->last_seen_ms = now_ms;
      p->seen = true;
      return true;
    }
  }
  if (have && is_ct_dev_type(dev_type)) {
    if (CtState *c = this->upsert_ct_(sn)) {
      fill_ct_(*c, body);
      c->dev_type = dev_type;
      c->last_seen_ms = now_ms;
      c->seen = true;
      return true;
    }
  }
  return false;
}

void JackeryState::apply_101_(JsonObjectConst body, uint32_t now_ms) {
  int query_dev_type = -1;
  bool has_query = to_int(body["devType"], query_dev_type);

  // Same selection as the HA integration: first non-empty array
  JsonArrayConst raw_plugs, raw_cts;
  for (const char *k : {"plug", "plugs", "socket", "sockets"}) {
    JsonVariantConst v = body[k];
    if (v.is<JsonArrayConst>() && v.as<JsonArrayConst>().size() > 0) {
      raw_plugs = v.as<JsonArrayConst>();
      break;
    }
  }
  for (const char *k : {"ct", "cts"}) {
    JsonVariantConst v = body[k];
    if (v.is<JsonArrayConst>() && v.as<JsonArrayConst>().size() > 0) {
      raw_cts = v.as<JsonArrayConst>();
      break;
    }
  }

  std::vector<PlugState> plug_items;
  if (!raw_plugs.isNull()) {
    for (JsonVariantConst iv : raw_plugs) {
      if (!iv.is<JsonObjectConst>())
        continue;
      JsonObjectConst item = iv.as<JsonObjectConst>();
      std::string sn = sub_sn(item);
      if (sn.empty() || sn == this->device_sn_)
        continue;
      PlugState ps;
      ps.sn = sn;
      ps.dev_type = 6;  // default when absent
      fill_plug_(ps, item);
      ps.last_seen_ms = now_ms;
      ps.seen = true;
      // later duplicates of the same SN overlay the first one
      bool dup = false;
      for (auto &e : plug_items) {
        if (e.sn == sn) {
          fill_plug_(e, item);
          dup = true;
          break;
        }
      }
      if (!dup)
        plug_items.push_back(ps);
    }
  }
  std::vector<CtState> ct_items;
  if (!raw_cts.isNull()) {
    for (JsonVariantConst iv : raw_cts) {
      if (!iv.is<JsonObjectConst>())
        continue;
      JsonObjectConst item = iv.as<JsonObjectConst>();
      std::string sn = sub_sn(item);
      if (sn.empty() || sn == this->device_sn_)
        continue;
      CtState cs;
      cs.sn = sn;
      fill_ct_(cs, item);
      cs.last_seen_ms = now_ms;
      cs.seen = true;
      bool dup = false;
      for (auto &e : ct_items) {
        if (e.sn == sn) {
          fill_ct_(e, item);
          dup = true;
          break;
        }
      }
      if (!dup)
        ct_items.push_back(cs);
    }
  }

  if (has_query && query_dev_type == 6) {
    // Full plug report: replaces the list (plugs missing from it have been unbound)
    if (plug_items.size() > 16)
      plug_items.resize(16);
    this->plugs_ = plug_items;
  } else if (has_query && query_dev_type == 2) {
    if (ct_items.size() > 4)
      ct_items.resize(4);
    this->cts_ = ct_items;
  } else {
    // devType not specified: merge by serial number
    for (auto &ps : plug_items) {
      if (PlugState *p = this->upsert_plug_(ps.sn)) {
        uint32_t keep_seen = ps.last_seen_ms;
        PlugState merged = *p;
        merged.dev_type = ps.dev_type;
        auto take = [](Opt &dst, const Opt &src) {
          if (src.has)
            dst = src;
        };
        take(merged.out_pw, ps.out_pw);
        take(merged.power_alias, ps.power_alias);
        take(merged.in_pw, ps.in_pw);
        take(merged.total_egy, ps.total_egy);
        take(merged.switch_sta, ps.switch_sta);
        take(merged.sys_switch, ps.sys_switch);
        if (ps.comm_mode >= 0)
          merged.comm_mode = ps.comm_mode;
        if (ps.comm_state >= 0)
          merged.comm_state = ps.comm_state;
        merged.last_seen_ms = keep_seen;
        merged.seen = true;
        *p = merged;
      }
    }
    for (auto &cs : ct_items) {
      if (CtState *c = this->upsert_ct_(cs.sn)) {
        CtState merged = *c;
        auto take = [](Opt &dst, const Opt &src) {
          if (src.has)
            dst = src;
        };
        take(merged.t_pw, cs.t_pw);
        take(merged.tn_pw, cs.tn_pw);
        take(merged.a_pw, cs.a_pw);
        take(merged.b_pw, cs.b_pw);
        take(merged.c_pw, cs.c_pw);
        take(merged.an_pw, cs.an_pw);
        take(merged.bn_pw, cs.bn_pw);
        take(merged.cn_pw, cs.cn_pw);
        take(merged.t_egy, cs.t_egy);
        take(merged.tn_egy, cs.tn_egy);
        if (cs.sub_type >= 0)
          merged.sub_type = cs.sub_type;
        if (cs.comm_state >= 0)
          merged.comm_state = cs.comm_state;
        merged.dev_type = cs.dev_type;
        merged.last_seen_ms = cs.last_seen_ms;
        merged.seen = true;
        *c = merged;
      }
    }
  }
}

JackeryState::Result JackeryState::ingest(JsonObjectConst root, uint32_t now_ms) {
  Result res;
  if (this->f_.size() != F_COUNT)
    this->f_.assign(F_COUNT, Opt{});

  int type = -1;
  const bool has_type = to_int(root["type"], type);
  JsonVariantConst body_v = root["body"];
  const bool body_null = body_v.isNull();
  const bool body_is_obj = body_v.is<JsonObjectConst>();

  std::string body_sn;
  if (body_is_obj)
    body_sn = str_field(body_v.as<JsonObjectConst>(), "deviceSn");
  const bool is_main = body_sn.empty() || body_sn == this->device_sn_ || body_sn == "system";
  if (is_main && has_type && (type == 2 || type == 23 || type == 25 || type == 106 || type == 107))
    this->capture_meta_(root, body_v);

  // Body missing: type 101 is ignored, anything else may be a "flat" status message (fields at the root)
  JsonObjectConst body;
  bool body_ok = false;  // body behaves like a (possibly empty) JSON object
  if (body_null) {
    if (has_type && type == 101)
      return res;
    bool flat = false;
    for (JsonPairConst kv : root) {
      if (is_flat_key(kv.key().c_str())) {
        flat = true;
        break;
      }
    }
    if (flat) {
      body = root;  // meta keys (type, eventId, ...) are not main fields, so merging the root is equivalent
      body_ok = true;
    } else {
      body_ok = true;  // empty dict
    }
  } else if (body_is_obj) {
    body = body_v.as<JsonObjectConst>();
    body_ok = true;
  }
  if (!body_ok)
    return res;
  const bool body_empty = body.isNull() || body.size() == 0;

  if (has_type && type == 23) {
    std::string sn = str_field(body, "deviceSn");
    if (sn.empty() || sn == this->device_sn_ || sn == "system") {
      res.main_updated = this->merge_main_(body, false);
    } else {
      this->touch_sub_(sn, now_ms);
      if (PlugState *p = this->find_plug_(sn)) {
        fill_plug_(*p, body);
        res.sub_updated = true;
      } else if (CtState *c = this->find_ct_(sn)) {
        fill_ct_(*c, body);
        res.sub_updated = true;
      }
    }
  } else if (has_type && (type == 106 || type == 107)) {
    res.main_updated = this->merge_main_(body, false);
  } else if (has_type && type == 102) {
    this->touch_sub_(sub_sn(body), now_ms);
    if (this->merge_arrays_(body, now_ms))
      res.sub_updated = true;
    else if (this->merge_point_(body, now_ms))
      res.sub_updated = true;
  } else if (has_type && type == 101) {
    this->apply_101_(body, now_ms);
    res.sub_updated = true;
  } else if (has_type && type == 123) {
    int code;
    if (to_int(body["errorCode"], code) && code == 401)
      res.token_error = true;
  } else if (!body_empty) {
    // type 25 and any other payload: host fields plus optional sub-device arrays / point updates
    this->touch_sub_(sub_sn(body), now_ms);
    for (const char *k : {"plugs", "plug", "cts"}) {
      JsonVariantConst v = body[k];
      if (v.is<JsonArrayConst>()) {
        for (JsonVariantConst iv : v.as<JsonArrayConst>()) {
          if (iv.is<JsonObjectConst>())
            this->touch_sub_(sub_sn(iv.as<JsonObjectConst>()), now_ms);
        }
      }
    }
    bool sub_updated = this->merge_arrays_(body, now_ms);
    bool point_updated = this->merge_point_(body, now_ms);
    bool main_nonempty = false;
    for (JsonPairConst kv : body) {
      if (!is_sub_key(kv.key().c_str())) {
        main_nonempty = true;
        break;
      }
    }
    if (main_nonempty && !(point_updated && !sub_updated))
      res.main_updated = this->merge_main_(body, true);
    res.sub_updated = sub_updated || point_updated;
  }

  if (res.main_updated)
    this->main_received_ = true;
  res.recognized = res.main_updated || res.sub_updated;
  this->recalc();
  return res;
}

// ---------------------------------------------------------------------------------------------------------------
// Derived values: port of the HA integration `_calculate_energy_flow`
// ---------------------------------------------------------------------------------------------------------------

void JackeryState::recalc() {
  if (this->f_.size() != F_COUNT)
    return;
  const auto &f = this->f_;
  auto present = [&](Field k) { return f[k].has; };
  auto val = [&](Field k) { return f[k].or0(); };

  Calc c;
  const double pv = val(F_PV_PW);
  const double grid_in = val(F_GRID_IN_PW);
  const double grid_out = val(F_GRID_OUT_PW);
  const double ongrid_charge = val(F_IN_ONGRID_PW);
  const double ongrid_supply = val(F_OUT_ONGRID_PW);
  const double in_side = val(F_IN_GRID_SIDE_PW);
  const double out_side = val(F_OUT_GRID_SIDE_PW);

  // Net power at the grid-tied port (HA `_effective_ongrid_net`)
  double p_ong;
  {
    std::vector<double> cand;
    if (present(F_GRID_IN_PW) || present(F_GRID_OUT_PW))
      cand.push_back(grid_in - grid_out);
    if (present(F_IN_ONGRID_PW) || present(F_OUT_ONGRID_PW))
      cand.push_back(ongrid_charge - ongrid_supply);
    if (present(F_IN_GRID_SIDE_PW) || present(F_OUT_GRID_SIDE_PW))
      cand.push_back(in_side - out_side);
    p_ong = pick_best(cand);
  }

  // AC socket
  const double ac_in = val(F_SW_EPS_IN_PW);
  const double ac_out = val(F_SW_EPS_OUT_PW);
  const double p_ac = ac_in - ac_out;
  const double ac_socket = ac_in > 0 ? ac_in : ac_out;

  // CT (first one), preferred over the system side estimation when its reading is usable
  bool grid_available = false;
  bool ct_available = false;
  double grid_buy = 0, grid_sell = 0;
  if (!this->cts_.empty()) {
    bool has_fields = false;
    ct_totals(this->cts_[0], grid_buy, grid_sell, has_fields);
    bool usable = false;
    if (std::fabs(grid_buy) > 0 || std::fabs(grid_sell) > 0)
      usable = true;
    else if (has_fields)
      usable = (this->cts_[0].comm_state == 1);
    if (usable) {
      ct_available = true;
      grid_available = true;
    }
  }

  // Net grid power
  double p_grid = 0;
  if (ct_available) {
    p_grid = grid_buy - grid_sell;
    if (ongrid_charge > 0 && grid_buy < ongrid_charge && (ongrid_charge - grid_buy) <= 50)
      p_grid = p_ong;
    else if (std::fabs(p_grid) < 1 && std::fabs(p_ong) > 1)
      p_grid = p_ong;
  } else {
    // HA `_grid_net_from_system`
    std::vector<double> cand;
    if (present(F_IN_GRID_SIDE_PW) || present(F_OUT_GRID_SIDE_PW))
      cand.push_back(in_side - out_side);
    if (present(F_GRID_IN_PW) || present(F_GRID_OUT_PW)) {
      double gi_go = grid_in - grid_out;
      if (std::fabs(gi_go) > 0 || grid_in != 0 || grid_out != 0)
        cand.push_back(gi_go);
    }
    if (present(F_IN_ONGRID_PW) || present(F_OUT_ONGRID_PW))
      cand.push_back(ongrid_charge - ongrid_supply);
    if (!cand.empty()) {
      p_grid = pick_best(cand);
      grid_available = true;
    }
  }

  // Battery power
  const double bat_in = val(F_BAT_IN_PW);
  const double bat_out = val(F_BAT_OUT_PW);
  double p_batt, batt_charge, batt_discharge;
  if (present(F_BAT_IN_PW) || present(F_BAT_OUT_PW)) {
    p_batt = bat_in - bat_out;
    batt_charge = bat_in;
    batt_discharge = bat_out;
  } else {
    p_batt = pv + p_ac + p_ong;
    batt_charge = std::max(0.0, p_batt);
    batt_discharge = std::max(0.0, -p_batt);
  }

  // Home load
  double p_home = 0;
  if (grid_available) {
    p_home = p_grid - p_ong;
    if (ct_available) {
      if (grid_buy > 0 && ongrid_charge > 0 && grid_buy < ongrid_charge && (ongrid_charge - grid_buy) <= 50)
        p_home = 0.0;
      else if (grid_buy > 0 && ongrid_charge > 0 && grid_buy < ongrid_charge && (ongrid_charge - grid_buy) > 50)
        p_home = ongrid_charge - grid_buy;
      else if (grid_sell > 0 && ongrid_supply > 0)
        p_home = grid_sell - ongrid_supply;
      else if (grid_sell > 0 && ongrid_charge > 0)
        p_home = grid_sell + ongrid_charge;
    }
  } else if (present(F_OUT_ONGRID_PW) && ongrid_supply > 0) {
    p_home = ongrid_supply;
  }
  const double other_load = val(F_OTHER_LOAD_PW);
  if (p_home == 0.0 && present(F_OTHER_LOAD_PW) && other_load > 0)
    p_home = other_load;

  c.valid = true;
  c.ac_socket_power = ac_socket;
  c.home_power = p_home;
  c.batt_net_power = p_batt;
  c.batt_charge_power = batt_charge;
  c.batt_discharge_power = batt_discharge;
  c.grid_net_power = p_grid;
  c.grid_available = grid_available;
  this->calc_ = c;
}

// ---------------------------------------------------------------------------------------------------------------
// Values for the entities
// ---------------------------------------------------------------------------------------------------------------

const PlugState *JackeryState::slot_plug_(uint8_t index) const {
  if (index >= MAX_PLUGS)
    return nullptr;
  if (!this->slot_sn_[index].empty()) {
    for (const auto &p : this->plugs_)
      if (p.sn == this->slot_sn_[index])
        return &p;
    return nullptr;
  }
  // Unpinned slot: the n-th discovered plug that no slot is pinned to, n = rank among the unpinned slots
  size_t rank = 0;
  for (uint8_t i = 0; i < index; i++)
    if (this->slot_sn_[i].empty())
      rank++;
  size_t seen = 0;
  for (const auto &p : this->plugs_) {
    bool pinned = false;
    for (const auto &s : this->slot_sn_)
      if (!s.empty() && s == p.sn)
        pinned = true;
    if (pinned)
      continue;
    if (seen == rank)
      return &p;
    seen++;
  }
  return nullptr;
}

const PlugState *JackeryState::plug(uint8_t index) const { return this->slot_plug_(index); }

void JackeryState::set_plug_sn(uint8_t index, const std::string &sn) {
  if (index < MAX_PLUGS)
    this->slot_sn_[index] = sn;
}

int JackeryState::plug_comm_mode(uint8_t index) const {
  const PlugState *p = this->slot_plug_(index);
  return p != nullptr ? p->comm_mode : -1;
}

bool JackeryState::plug_fresh(uint8_t index, uint32_t now_ms, uint32_t timeout_ms) const {
  const PlugState *p = this->slot_plug_(index);
  if (p == nullptr || !p->seen)
    return false;
  return static_cast<uint32_t>(now_ms - p->last_seen_ms) <= timeout_ms;
}

bool JackeryState::ct_fresh(uint32_t now_ms, uint32_t timeout_ms) const {
  const CtState *c = this->ct();
  if (c == nullptr || !c->seen)
    return false;
  return static_cast<uint32_t>(now_ms - c->last_seen_ms) <= timeout_ms;
}

bool JackeryState::sensor_value(SensorKind kind, uint8_t index, float &out) const {
  if (this->f_.size() != F_COUNT)
    return false;
  auto raw = [&](Field k, double scale, bool as_int = false) -> bool {
    if (!this->f_[k].has)
      return false;
    double v = this->f_[k].v;
    if (as_int)
      v = static_cast<double>(static_cast<long long>(v));
    out = static_cast<float>(v * scale);
    return true;
  };
  auto calc = [&](double v) -> bool {
    if (!this->calc_.valid || !this->main_received_)
      return false;
    out = static_cast<float>(v);
    return true;
  };

  switch (kind) {
    case SensorKind::STATUS_CODE:
      return raw(F_STAT, 1.0);
    case SensorKind::WORK_MODE_CODE:
      return raw(F_WORK_MODE, 1.0);
    case SensorKind::SOLAR_POWER:
      return raw(F_PV_PW, 1.0);
    case SensorKind::SOLAR_ENERGY:
      return raw(F_PV_EGY, 0.01);
    case SensorKind::PV_TO_BATTERY_ENERGY:
      return raw(F_PV_OT_BAT_EGY, 0.01);
    case SensorKind::PV_TO_SOCKET_ENERGY:
      return raw(F_PV_OT_AC_EGY, 0.01);
    case SensorKind::PV_TO_GRID_ENERGY:
      return raw(F_PV_OT_ONGRID_EGY, 0.01);
    case SensorKind::GRID_TO_SOCKET_ENERGY:
      return raw(F_ONGRID_OT_AC_LOAD_EGY, 0.01);
    case SensorKind::GRID_TO_BATTERY_ENERGY:
      return raw(F_ONGRID_OT_BAT_EGY, 0.01);
    case SensorKind::BATTERY_TO_SOCKET_ENERGY:
      return raw(F_BAT_OT_AC_EGY, 0.01);
    case SensorKind::BATTERY_TO_GRID_ENERGY:
      return raw(F_BAT_OT_GRID_EGY, 0.01);
    case SensorKind::SOCKET_TO_BATTERY_ENERGY:
      return raw(F_AC_OT_BAT_EGY, 0.01);
    case SensorKind::SOCKET_TO_GRID_ENERGY:
      return raw(F_AC_OT_ONGRID_EGY, 0.01);
    case SensorKind::PV_POWER:
      if (index >= MAX_PV_CHANNELS)
        return false;
      return raw(static_cast<Field>(F_PV1 + index), 1.0);
    case SensorKind::PV_ENERGY:
      if (index >= MAX_PV_CHANNELS)
        return false;
      return raw(static_cast<Field>(F_PV1_EGY + index), 0.01);
    case SensorKind::AC_GRID_INPUT_POWER:
      return raw(F_GRID_IN_PW, 1.0);
    case SensorKind::AC_GRID_OUTPUT_POWER:
      return raw(F_GRID_OUT_PW, 1.0);
    case SensorKind::AC_GRID_INPUT_ENERGY:
      return raw(F_IN_ONGRID_EGY, 0.01);
    case SensorKind::AC_GRID_OUTPUT_ENERGY:
      return raw(F_OUT_ONGRID_EGY, 0.01);
    case SensorKind::AC_GRID_POWER:
      return this->calc_.grid_available && calc(this->calc_.grid_net_power);
    case SensorKind::AC_HOME_POWER:
      return calc(this->calc_.home_power);
    case SensorKind::AC_OTHER_LOAD_POWER:
      return raw(F_OTHER_LOAD_PW, 1.0);
    case SensorKind::AC_MAX_FEED_IN_POWER:
      return raw(F_MAX_FEED_GRID, 1.0);
    case SensorKind::AC_SOCKET_POWER:
      return calc(this->calc_.ac_socket_power);
    case SensorKind::AC_SOCKET_INPUT_POWER:
      return raw(F_SW_EPS_IN_PW, 1.0);
    case SensorKind::AC_SOCKET_INPUT_ENERGY:
      return raw(F_IN_EPS_EGY, 0.01);
    case SensorKind::AC_SOCKET_OUTPUT_ENERGY:
      return raw(F_OUT_EPS_EGY, 0.01);
    case SensorKind::BATTERY_SOC:
      return raw(F_BAT_SOC, 1.0, true);
    case SensorKind::BATTERY_AVERAGE_SOC:
      return raw(F_SOC, 1.0, true);
    case SensorKind::BATTERY_TEMPERATURE:
      return raw(F_CELL_TEMP, 0.1);
    case SensorKind::BATTERY_PACK_COUNT:
      return raw(F_BAT_NUM, 1.0, true);
    case SensorKind::BATTERY_CHARGE_POWER:
      return calc(this->calc_.batt_charge_power);
    case SensorKind::BATTERY_DISCHARGE_POWER:
      return calc(this->calc_.batt_discharge_power);
    case SensorKind::BATTERY_NET_POWER:
      return calc(this->calc_.batt_net_power);
    case SensorKind::BATTERY_CHARGE_ENERGY:
      return raw(F_BAT_CHG_EGY, 0.01);
    case SensorKind::BATTERY_DISCHARGE_ENERGY:
      return raw(F_BAT_DISCHG_EGY, 0.01);

    case SensorKind::CT_FORWARD_POWER:
    case SensorKind::CT_REVERSE_POWER:
    case SensorKind::CT_FORWARD_ENERGY:
    case SensorKind::CT_REVERSE_ENERGY:
    case SensorKind::CT_PHASE_A_FORWARD_POWER:
    case SensorKind::CT_PHASE_B_FORWARD_POWER:
    case SensorKind::CT_PHASE_C_FORWARD_POWER:
    case SensorKind::CT_PHASE_A_REVERSE_POWER:
    case SensorKind::CT_PHASE_B_REVERSE_POWER:
    case SensorKind::CT_PHASE_C_REVERSE_POWER: {
      if (index != 0 || this->cts_.empty())
        return false;
      const CtState &c = this->cts_[0];
      double buy, sell;
      bool has;
      ct_totals(c, buy, sell, has);
      auto opt = [&](const Opt &o, double scale) {
        if (!o.has)
          return false;
        out = static_cast<float>(o.v * scale);
        return true;
      };
      switch (kind) {
        case SensorKind::CT_FORWARD_POWER:
          if (!has)
            return false;
          out = static_cast<float>(buy);
          return true;
        case SensorKind::CT_REVERSE_POWER:
          if (!has)
            return false;
          out = static_cast<float>(sell);
          return true;
        case SensorKind::CT_FORWARD_ENERGY:
          return opt(c.t_egy, 0.01);
        case SensorKind::CT_REVERSE_ENERGY:
          return opt(c.tn_egy, 0.01);
        case SensorKind::CT_PHASE_A_FORWARD_POWER:
          return opt(c.a_pw, 1.0);
        case SensorKind::CT_PHASE_B_FORWARD_POWER:
          return opt(c.b_pw, 1.0);
        case SensorKind::CT_PHASE_C_FORWARD_POWER:
          return opt(c.c_pw, 1.0);
        case SensorKind::CT_PHASE_A_REVERSE_POWER:
          return opt(c.an_pw, 1.0);
        case SensorKind::CT_PHASE_B_REVERSE_POWER:
          return opt(c.bn_pw, 1.0);
        default:
          return opt(c.cn_pw, 1.0);
      }
    }

    case SensorKind::PLUG_POWER: {
      const PlugState *p = this->slot_plug_(index);
      if (p == nullptr || !p->power().has)
        return false;
      out = static_cast<float>(p->power().v);
      return true;
    }
    case SensorKind::PLUG_ENERGY: {
      const PlugState *p = this->slot_plug_(index);
      if (p == nullptr || !p->total_egy.has)
        return false;
      out = static_cast<float>(p->total_egy.v * 0.01);
      return true;
    }
  }
  return false;
}

bool JackeryState::binary_value(BinaryKind kind, bool &out) const {
  if (this->f_.size() != F_COUNT)
    return false;
  Field k;
  switch (kind) {
    case BinaryKind::ON_GRID:
      k = F_ONGRID_STAT;
      break;
    case BinaryKind::CT_ONLINE:
      k = F_CT_STAT;
      break;
    case BinaryKind::GRID_METER_LINK:
      k = F_GRID_SATE;
      break;
    case BinaryKind::SOCKET_OK:
      k = F_SW_EPS_STATE;
      break;
    default:
      return false;
  }
  if (!this->f_[k].has)
    return false;
  out = static_cast<int>(this->f_[k].v) == 1;
  return true;
}

bool JackeryState::text_value(TextKind kind, uint8_t index, std::string &out) const {
  char buf[48];
  switch (kind) {
    case TextKind::STATUS: {
      if (this->f_.size() != F_COUNT || !this->f_[F_STAT].has)
        return false;
      int v = static_cast<int>(this->f_[F_STAT].v);
      if (v >= 0 && v < 6) {
        out = DEVICE_STATUS_NAMES[v];
      } else {
        snprintf(buf, sizeof(buf), "Unknown (%d)", v);
        out = buf;
      }
      return true;
    }
    case TextKind::WORK_MODE: {
      if (this->f_.size() != F_COUNT || !this->f_[F_WORK_MODE].has)
        return false;
      int v = static_cast<int>(this->f_[F_WORK_MODE].v);
      if (v >= 0 && v < 8) {
        out = WORK_MODE_NAMES[v];
      } else {
        snprintf(buf, sizeof(buf), "Unknown (%d)", v);
        out = buf;
      }
      return true;
    }
    case TextKind::FIRMWARE_VERSION:
      if (this->soft_ver_.empty())
        return false;
      out = this->soft_ver_;
      return true;
    case TextKind::MODEL:
      if (this->model_name_.empty())
        return false;
      out = this->model_name_;
      return true;
    case TextKind::CT_TYPE: {
      if (index != 0 || this->cts_.empty() || this->cts_[0].sub_type < 0)
        return false;
      int v = this->cts_[0].sub_type;
      if (v >= 1 && v <= 7) {
        out = CT_SUBTYPE_NAMES[v - 1];
      } else {
        snprintf(buf, sizeof(buf), "Unknown (%d)", v);
        out = buf;
      }
      return true;
    }
  }
  return false;
}

bool JackeryState::switch_value(SwitchKind kind, uint8_t index, bool &out) const {
  switch (kind) {
    case SwitchKind::AC_SOCKET:
    case SwitchKind::AUTO_STANDBY_ALLOWED: {
      if (this->f_.size() != F_COUNT)
        return false;
      Field k = (kind == SwitchKind::AC_SOCKET) ? F_SW_EPS : F_IS_AUTO_STANDBY;
      if (!this->f_[k].has)
        return false;
      out = static_cast<int>(this->f_[k].v) != 0;  // HA: bool(int(val))
      return true;
    }
    case SwitchKind::PLUG: {
      const PlugState *p = this->slot_plug_(index);
      if (p == nullptr || !p->switch_state().has)
        return false;
      out = static_cast<int>(p->switch_state().v) != 0;
      return true;
    }
  }
  return false;
}

bool JackeryState::number_value(NumberKind kind, float &out) const {
  if (this->f_.size() != F_COUNT)
    return false;
  Field k;
  switch (kind) {
    case NumberKind::SOC_CHARGE_LIMIT:
      k = F_SOC_CHG_LIMIT;
      break;
    case NumberKind::SOC_DISCHARGE_LIMIT:
      k = F_SOC_DISCHG_LIMIT;
      break;
    case NumberKind::MAX_OUTPUT_POWER:
      k = F_MAX_OUT_PW;
      break;
    default:
      return false;
  }
  if (!this->f_[k].has)
    return false;
  out = static_cast<float>(this->f_[k].v);
  return true;
}

bool JackeryState::number_bounds(NumberKind kind, float &min_v, float &max_v) const {
  if (this->f_.size() != F_COUNT)
    return false;
  Field lo, hi;
  switch (kind) {
    case NumberKind::SOC_CHARGE_LIMIT:
      lo = F_MIN_SOC_CHG;
      hi = F_MAX_SOC_CHG;
      break;
    case NumberKind::SOC_DISCHARGE_LIMIT:
      lo = F_MIN_SOC_DISCHG;
      hi = F_MAX_SOC_DISCHG;
      break;
    default:
      return false;
  }
  bool any = false;
  if (this->f_[lo].has) {
    min_v = static_cast<float>(this->f_[lo].v);
    any = true;
  }
  if (this->f_[hi].has) {
    max_v = static_cast<float>(this->f_[hi].v);
    any = true;
  }
  return any;
}

bool JackeryState::select_index(SelectKind kind, size_t &out) const {
  (void) kind;
  if (this->f_.size() != F_COUNT || !this->f_[F_AUTO_STANDBY].has)
    return false;
  int v = static_cast<int>(this->f_[F_AUTO_STANDBY].v);
  if (v < 0 || v > 2)
    return false;
  out = static_cast<size_t>(v);
  return true;
}

}  // namespace esphome::jackerysv3
