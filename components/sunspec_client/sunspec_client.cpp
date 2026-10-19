#include "sunspec_client.h"

#include <cinttypes>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <limits>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::sunspec_client {

static const char *const TAG = "sunspec_client";

// Register addresses where the "SunS" marker is looked for, in this order (0-based PDU addresses:
// register 40001 in the 1-based notation of the SunSpec documents is address 40000 here).
static constexpr uint16_t BASE_CANDIDATES[] = {40000, 0, 50000};

/// Informative label of a model id (the list is not exhaustive).
static const char *model_name(uint16_t id) {
  switch (id) {
    case 1:
      return "Common";
    case 11:
      return "Ethernet link layer";
    case 12:
      return "IPv4";
    case 13:
      return "IPv6";
    case 101:
      return "Inverter, single phase";
    case 102:
      return "Inverter, split phase";
    case 103:
      return "Inverter, three phase";
    case 111:
      return "Inverter, single phase (float)";
    case 112:
      return "Inverter, split phase (float)";
    case 113:
      return "Inverter, three phase (float)";
    case 120:
      return "Nameplate";
    case 121:
      return "Basic settings";
    case 122:
      return "Extended measurements and status";
    case 123:
      return "Immediate controls";
    case 124:
      return "Basic storage controls";
    case 160:
      return "Multiple MPPT inverter extension";
    case 201:
      return "Meter, single phase";
    case 202:
      return "Meter, split phase";
    case 203:
      return "Meter, wye three phase";
    case 204:
      return "Meter, delta three phase";
    case 802:
      return "Battery base";
    default:
      return id >= 64000 ? "Vendor specific" : "Unknown";
  }
}

void SunSpecClient::setup() {
  // Wait a little so that the link below the bus (WiFi, TCP) is up before the first request.
  this->set_timeout("scan_start", this->startup_delay_ms_, [this]() { this->start_scan_(); });
}

void SunSpecClient::dump_config() {
  ESP_LOGCONFIG(TAG, "SunSpec client:");
  ESP_LOGCONFIG(TAG, "  Unit address: %u", static_cast<unsigned>(this->address_));
  ESP_LOGCONFIG(TAG, "  Startup delay: %" PRIu32 " ms", this->startup_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Scan retry interval: %" PRIu32 " ms", this->scan_retry_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Update interval: %" PRIu32 " ms", this->update_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Max modules (repeating groups): %u", static_cast<unsigned>(this->max_modules_));
  ESP_LOGCONFIG(TAG, "  Name prefix: '%s'", this->name_prefix_.c_str());
  std::string wanted;
  for (uint16_t id : this->wanted_models_) {
    if (!wanted.empty())
      wanted += ", ";
    wanted += std::to_string(id);
  }
  ESP_LOGCONFIG(TAG, "  Wanted models: %s", wanted.empty() ? "(none)" : wanted.c_str());
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Models", this->models_text_sensor_);
#endif
}

void SunSpecClient::start_scan_() {
  this->models_.clear();
  this->scan_done_ = false;
  this->base_index_ = 0;
  this->retries_ = 0;
  this->state_ = ScanState::PROBING_BASE;
  ESP_LOGI(TAG, "Scanning the SunSpec models of unit %u", static_cast<unsigned>(this->address_));
  this->publish_models_text_("scanning");
  this->probe_base_();
}

void SunSpecClient::probe_base_() {
  if (this->base_index_ >= std::size(BASE_CANDIDATES)) {
    this->fail_scan_("no SunSpec marker found at registers 40000, 0 or 50000");
    return;
  }
  this->state_ = ScanState::PROBING_BASE;
  this->request_(BASE_CANDIDATES[this->base_index_], 2);
}

void SunSpecClient::read_header_(uint16_t address) {
  this->state_ = ScanState::READING_HEADER;
  this->request_(address, 2);
}

bool SunSpecClient::request_(uint16_t address, uint16_t count) {
  this->pending_address_ = address;
  if (!this->read_holding_registers(address, count)) {
    this->abort_cycle_("request refused by the Modbus hub");
    return false;
  }
  return true;
}

void SunSpecClient::on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                              modbus::ResponseStatus status) {
  if (this->state_ == ScanState::IDLE)
    return;
  if (start_address != this->pending_address_) {
    ESP_LOGW(TAG, "Unexpected reply for register %u (waiting for %u), ignored", start_address,
             this->pending_address_);
    return;
  }
  this->retries_ = 0;

  if (this->state_ == ScanState::READING_MODEL) {
    if (!modbus::succeeded(status) || registers.empty()) {
      this->abort_cycle_("reading a model failed");
      return;
    }
    this->buffer_.insert(this->buffer_.end(), registers.begin(), registers.end());
    if (this->buffer_.size() < this->buffer_target_)
      this->request_chunk_();
    else
      this->on_model_read_();
    return;
  }

  if (this->state_ == ScanState::PROBING_BASE) {
    if (!modbus::succeeded(status) || registers.size() < 2 || registers[0] != SUNSPEC_MARKER_HI ||
        registers[1] != SUNSPEC_MARKER_LO) {
      ESP_LOGD(TAG, "No SunSpec marker at register %u", start_address);
      this->base_index_++;
      this->probe_base_();
      return;
    }
    ESP_LOGI(TAG, "SunSpec marker found at register %u", start_address);
    this->read_header_(static_cast<uint16_t>(start_address + 2));
    return;
  }

  // READING_HEADER
  if (!modbus::succeeded(status) || registers.size() < 2) {
    this->fail_scan_("reading a model header failed");
    return;
  }
  const uint16_t id = registers[0];
  const uint16_t length = registers[1];
  if (id == SUNSPEC_END_MODEL_ID) {
    this->finish_scan_();
    return;
  }
  if (id == 0) {
    ESP_LOGW(TAG, "Invalid model id 0 at register %u, end of the chain assumed", start_address);
    this->finish_scan_();
    return;
  }
  this->models_.push_back(ModelInfo{id, length, start_address});
  if (this->models_.size() >= MAX_SCANNED_MODELS) {
    ESP_LOGW(TAG, "More than %u models, the scan stops here", static_cast<unsigned>(MAX_SCANNED_MODELS));
    this->finish_scan_();
    return;
  }
  const uint32_t next = static_cast<uint32_t>(start_address) + 2u + length;
  if (next > 0xFFFDu) {
    this->fail_scan_("the chain of models runs past the register space");
    return;
  }
  this->read_header_(static_cast<uint16_t>(next));
}

bool SunSpecClient::on_no_response(std::span<const uint8_t> request_pdu) {
  if (this->state_ == ScanState::IDLE)
    return false;
  if (this->retries_ < MAX_NO_RESPONSE_RETRIES) {
    this->retries_++;
    ESP_LOGW(TAG, "No answer for register %u, retry %u/%u", this->pending_address_,
             static_cast<unsigned>(this->retries_), static_cast<unsigned>(MAX_NO_RESPONSE_RETRIES));
    return true;  // the hub re-queues the frame
  }
  this->abort_cycle_("no answer from the device");
  return false;
}

void SunSpecClient::finish_scan_() {
  this->state_ = ScanState::IDLE;
  this->scan_done_ = true;
  ESP_LOGI(TAG, "Scan done: %u model(s)", static_cast<unsigned>(this->models_.size()));
  std::string text;
  for (const auto &model : this->models_) {
    ESP_LOGI(TAG, "  model %u (%s): %u registers, header at register %u", model.id, model_name(model.id),
             model.length, model.address);
    if (!text.empty())
      text += ", ";
    text += std::to_string(model.id);
  }
  if (text.size() > MAX_TEXT_STATE_LENGTH) {
    text.resize(MAX_TEXT_STATE_LENGTH - 3);
    text += "...";
  }
  if (text.empty())
    text = "none";
  this->publish_models_text_(text);

  // Models to read: wanted by the YAML, exposed by the device (and known by the tables).
  this->plan_.clear();
  for (size_t i = 0; i < this->models_.size(); i++) {
    const uint16_t id = this->models_[i].id;
    const bool wanted = std::find(this->wanted_models_.begin(), this->wanted_models_.end(), id) != this->wanted_models_.end();
    if (wanted && find_model_def(id) != nullptr)
      this->plan_.push_back(static_cast<uint8_t>(i));
  }
  for (uint16_t id : this->wanted_models_) {
    const bool found = std::any_of(this->models_.begin(), this->models_.end(),
                                   [id](const ModelInfo &m) { return m.id == id; });
    if (!found)
      ESP_LOGW(TAG, "Model %u is wanted but the device does not expose it", id);
  }
  if (this->plan_.empty()) {
    ESP_LOGW(TAG, "No wanted model is exposed by the device, no entity will be created");
    return;
  }
  this->start_cycle_();
}

void SunSpecClient::fail_scan_(const char *reason) {
  this->state_ = ScanState::IDLE;
  ESP_LOGE(TAG, "Scan failed: %s. New attempt in %" PRIu32 " ms", reason, this->scan_retry_interval_ms_);
  this->publish_models_text_("scan failed");
  this->set_timeout("scan_start", this->scan_retry_interval_ms_, [this]() { this->start_scan_(); });
}

void SunSpecClient::publish_models_text_(const std::string &text) {
#ifdef USE_TEXT_SENSOR
  if (this->models_text_sensor_ != nullptr)
    this->models_text_sensor_->publish_state(text);
#endif
}

// ---------------------------------------------------------------------------------------------------------
// Reading of the models

void SunSpecClient::abort_cycle_(const char *reason) {
  if (!this->entities_created_) {
    this->fail_scan_(reason);
    return;
  }
  // The entities exist: a failure only costs this update, the next cycle tries again.
  this->state_ = ScanState::IDLE;
  ESP_LOGW(TAG, "Update cycle aborted: %s", reason);
  this->schedule_next_cycle_();
}

void SunSpecClient::schedule_next_cycle_() {
  this->set_timeout("cycle", this->update_interval_ms_, [this]() { this->start_cycle_(); });
}

void SunSpecClient::start_cycle_() {
  this->plan_pos_ = 0;
  this->retries_ = 0;
  this->read_next_model_();
}

void SunSpecClient::read_next_model_() {
  if (this->plan_pos_ >= this->plan_.size()) {
    this->state_ = ScanState::IDLE;
    if (!this->entities_created_) {
      this->create_entities_();
      // Publish the first values right away.
      this->start_cycle_();
    } else {
      this->schedule_next_cycle_();
    }
    return;
  }
  const ModelInfo &model = this->models_[this->plan_[this->plan_pos_]];
  uint32_t total = static_cast<uint32_t>(model.length) + 2u;
  if (total > MAX_MODEL_REGISTERS) {
    ESP_LOGW(TAG, "Model %u has %u registers, only the first %u are read", model.id, static_cast<unsigned>(total),
             static_cast<unsigned>(MAX_MODEL_REGISTERS));
    total = MAX_MODEL_REGISTERS;
  }
  this->buffer_target_ = static_cast<uint16_t>(total);
  this->buffer_.clear();
  this->buffer_.reserve(total);
  this->state_ = ScanState::READING_MODEL;
  this->request_chunk_();
}

void SunSpecClient::request_chunk_() {
  const ModelInfo &model = this->models_[this->plan_[this->plan_pos_]];
  const uint16_t done = static_cast<uint16_t>(this->buffer_.size());
  const uint16_t count = std::min<uint16_t>(MAX_REGISTERS_PER_REQUEST, this->buffer_target_ - done);
  this->request_(static_cast<uint16_t>(model.address + done), count);
}

void SunSpecClient::on_model_read_() {
  const uint8_t index = this->plan_[this->plan_pos_];
  if (!this->entities_created_)
    this->discover_points_(index);
  else
    this->publish_values_(index);
  this->plan_pos_++;
  this->read_next_model_();
}

static bool is_numeric(PointType t) {
  switch (t) {
    case PointType::INT16:
    case PointType::UINT16:
    case PointType::INT32:
    case PointType::UINT32:
    case PointType::INT64:
    case PointType::UINT64:
    case PointType::ACC32:
    case PointType::ACC64:
    case PointType::FLOAT32:
      return true;
    default:
      return false;
  }
}

static bool is_text(PointType t) {
  return t == PointType::ENUM16 || t == PointType::BITFIELD16 || t == PointType::BITFIELD32 ||
         t == PointType::STRING;
}

/// Text of an enum, bitfield or string point. Returns false when the point is not implemented by the device
/// (reserved value of its type, empty string) or outside the registers read.
static bool decode_text(const std::vector<uint16_t> &regs, const PointDef &def, uint16_t offset, std::string &out) {
  if (static_cast<size_t>(offset) + def.size > regs.size())
    return false;
  const uint16_t *r = &regs[offset];
  out.clear();
  if (def.type == PointType::STRING) {
    // Two characters per register, high byte first, padded with NUL
    for (uint8_t i = 0; i < def.size; i++) {
      for (int shift = 8; shift >= 0; shift -= 8) {
        const char c = static_cast<char>((r[i] >> shift) & 0xFF);
        if (c == '\0')
          goto end_of_string;
        out += c;
      }
    }
  end_of_string:
    while (!out.empty() && out.back() == ' ')
      out.pop_back();
    return !out.empty();
  }
  if (def.type == PointType::ENUM16) {
    if (r[0] == 0xFFFF)
      return false;
    for (uint8_t i = 0; i < def.num_symbols; i++) {
      if (def.symbols[i].value == r[0]) {
        out = def.symbols[i].name;
        return true;
      }
    }
    out = std::to_string(r[0]);
    return true;
  }
  // Bitfield: names of the bits set
  uint32_t bits = r[0];
  uint8_t nbits = 16;
  if (def.type == PointType::BITFIELD32) {
    bits = (static_cast<uint32_t>(r[0]) << 16) | r[1];
    nbits = 32;
  }
  if (bits == (nbits == 16 ? 0xFFFFu : 0xFFFFFFFFu))
    return false;
  for (uint8_t bit = 0; bit < nbits; bit++) {
    if (!(bits & (1u << bit)))
      continue;
    if (!out.empty())
      out += ", ";
    const char *name = nullptr;
    for (uint8_t i = 0; i < def.num_symbols; i++) {
      if (def.symbols[i].value == bit)
        name = def.symbols[i].name;
    }
    out += name != nullptr ? std::string(name) : "bit " + std::to_string(bit);
  }
  if (out.empty())
    out = "none";
  if (out.size() > MAX_TEXT_STATE_LENGTH) {
    out.resize(MAX_TEXT_STATE_LENGTH - 3);
    out += "...";
  }
  return true;
}

/// Value of a point. Returns false when the point is not implemented by the device (reserved "not
/// implemented" value of its type, or scale factor not implemented) or outside the registers read.
/// An accumulator that reads 0 is "not accumulated": implemented only once it counts (checked at discovery).
static bool decode_point(const std::vector<uint16_t> &regs, const PointDef &def, uint16_t offset, int16_t sf_offset,
                         bool discovery, float &value) {
  if (static_cast<size_t>(offset) + def.size > regs.size())
    return false;
  const uint16_t *r = &regs[offset];
  switch (def.type) {
    case PointType::INT16:
      if (r[0] == 0x8000)
        return false;
      value = static_cast<float>(static_cast<int16_t>(r[0]));
      break;
    case PointType::UINT16:
      if (r[0] == 0xFFFF)
        return false;
      value = static_cast<float>(r[0]);
      break;
    case PointType::INT32: {
      const uint32_t u = (static_cast<uint32_t>(r[0]) << 16) | r[1];
      if (u == 0x80000000u)
        return false;
      value = static_cast<float>(static_cast<int32_t>(u));
      break;
    }
    case PointType::UINT32:
    case PointType::ACC32: {
      const uint32_t u = (static_cast<uint32_t>(r[0]) << 16) | r[1];
      if (u == 0xFFFFFFFFu || (def.type == PointType::ACC32 && discovery && u == 0))
        return false;
      value = static_cast<float>(u);
      break;
    }
    case PointType::INT64:
    case PointType::UINT64:
    case PointType::ACC64: {
      uint64_t u = 0;
      for (int i = 0; i < 4; i++)
        u = (u << 16) | r[i];
      if (def.type == PointType::INT64) {
        if (u == 0x8000000000000000ull)
          return false;
        value = static_cast<float>(static_cast<int64_t>(u));
      } else {
        if (u == 0xFFFFFFFFFFFFFFFFull || (def.type == PointType::ACC64 && discovery && u == 0))
          return false;
        value = static_cast<float>(u);
      }
      break;
    }
    case PointType::FLOAT32: {
      const uint32_t u = (static_cast<uint32_t>(r[0]) << 16) | r[1];
      float f;
      static_assert(sizeof(f) == sizeof(u));
      memcpy(&f, &u, sizeof(f));
      if (std::isnan(f))
        return false;
      value = f;
      break;
    }
    default:
      return false;
  }
  if (sf_offset >= 0) {
    if (static_cast<size_t>(sf_offset) >= regs.size() || regs[sf_offset] == 0x8000)
      return false;
    value *= std::pow(10.0f, static_cast<float>(static_cast<int16_t>(regs[sf_offset])));
  }
  return true;
}

void SunSpecClient::discover_points_(uint8_t model_index) {
  const ModelInfo &info = this->models_[model_index];
  const ModelDef *def = find_model_def(info.id);
  if (def == nullptr)
    return;

  auto add = [&](const PointDef &pt, uint16_t offset, uint8_t instance, bool in_group) {
    if (is_text(pt.type)) {
      std::string text;
      if (!decode_text(this->buffer_, pt, offset, text))
        return;  // not implemented by this device: no entity
      Binding b{};
      b.model_index = model_index;
      b.def = &pt;
      b.offset = offset;
      b.sf_offset = -1;
      b.instance = instance;
      b.in_group = in_group;
      this->bindings_.push_back(b);
      return;
    }
    if (!is_numeric(pt.type))
      return;
    float value;
    if (!decode_point(this->buffer_, pt, offset, pt.sf_offset, true, value))
      return;  // not implemented by this device: no entity
    uint8_t decimals = 0;
    if (pt.type == PointType::FLOAT32) {
      decimals = 2;
    } else if (pt.sf_offset >= 0) {
      const int16_t sf = static_cast<int16_t>(this->buffer_[pt.sf_offset]);
      decimals = sf < 0 ? static_cast<uint8_t>(std::min(-static_cast<int>(sf), 4)) : 0;
    }
    Binding b{};
    b.model_index = model_index;
    b.def = &pt;
    b.offset = offset;
    b.sf_offset = pt.sf_offset;
    b.instance = instance;
    b.in_group = in_group;
    b.decimals = decimals;
    this->bindings_.push_back(b);
  };

  for (uint8_t i = 0; i < def->num_points; i++)
    add(def->points[i], def->points[i].offset, 0, false);

  if (def->group != nullptr) {
    const GroupDef &g = *def->group;
    uint16_t n = g.count_offset < this->buffer_.size() ? this->buffer_[g.count_offset] : 0;
    if (n > this->max_modules_) {
      ESP_LOGW(TAG, "Model %u: the device has %u %s instances, only %u get entities (max_modules)", info.id, n,
               g.name, static_cast<unsigned>(this->max_modules_));
      n = this->max_modules_;
    }
    for (uint16_t inst = 0; inst < n; inst++) {
      const uint16_t base = static_cast<uint16_t>(g.first_offset + inst * g.stride);
      for (uint8_t i = 0; i < g.num_points; i++)
        add(g.points[i], static_cast<uint16_t>(base + g.points[i].offset), static_cast<uint8_t>(inst), true);
    }
  }
}

std::string SunSpecClient::make_name_(const PointDef &def, const ModelDef &model, const char *group_name,
                                      uint8_t instance) const {
  std::string name = this->name_prefix_;
  if (!name.empty())
    name += ' ';
  if (group_name != nullptr) {
    name += group_name;
    name += ' ';
    name += std::to_string(instance + 1);
    name += ' ';
  }
  name += def.label;
  return name;
}

uint8_t SunSpecClient::unit_index_(const char *unit) const {
  if (unit[0] == '\0')
    return 0;
  for (const auto &u : this->unit_indices_) {
    if (strcmp(u.unit, unit) == 0)
      return u.index;
  }
  return 0;
}

void SunSpecClient::create_entities_() {
  size_t created = 0, skipped = 0;
  for (auto &b : this->bindings_) {
    const ModelInfo &info = this->models_[b.model_index];
    const ModelDef *def = find_model_def(info.id);
    std::string name = this->make_name_(*b.def, *def, b.in_group ? def->group->name : nullptr, b.instance);
    // Several models can carry the same label (e.g. 101 and 103): keep names unique.
    for (const std::string *used : this->names_) {
      if (*used == name) {
        name += " (model " + std::to_string(info.id) + ")";
        break;
      }
    }
    if (is_text(b.def->type)) {
#ifdef USE_TEXT_SENSOR
      if (App.get_text_sensors().size() >= ESPHOME_ENTITY_TEXT_SENSOR_COUNT) {
        skipped++;
        continue;
      }
      auto *ts = new text_sensor::TextSensor();  // NOLINT: entities live for the whole runtime, never freed
      auto *stored = new std::string(name);      // NOLINT: same lifetime as the entity
      this->names_.push_back(stored);
      const uint32_t hash = fnv1_hash_object_id(stored->c_str(), stored->size());
      // Diagnostic category: these are status, events and identification strings
      const uint32_t fields = static_cast<uint32_t>(ENTITY_CATEGORY_DIAGNOSTIC) << ENTITY_FIELD_ENTITY_CATEGORY_SHIFT;
      App.register_text_sensor(ts, stored->c_str(), hash, fields);
      b.text_sensor = ts;
      created++;
#endif
#ifdef USE_BINARY_SENSOR
      if (this->bitfield_binary_sensors_ && b.def->symbols != nullptr &&
          (b.def->type == PointType::BITFIELD16 || b.def->type == PointType::BITFIELD32)) {
        for (uint8_t i = 0; i < b.def->num_symbols; i++) {
          const SymbolDef &sym = b.def->symbols[i];
          if (strncmp(sym.name, "RESERVED", 8) == 0)
            continue;
          if (App.get_binary_sensors().size() >= ESPHOME_ENTITY_BINARY_SENSOR_COUNT) {
            skipped++;
            continue;
          }
          // "GROUND_FAULT" -> "Ground fault"
          std::string bit_name = name + " ";
          const size_t start = bit_name.size();
          bit_name += sym.name;
          for (size_t k = start; k < bit_name.size(); k++) {
            if (bit_name[k] == '_')
              bit_name[k] = ' ';
            else if (k > start && bit_name[k] >= 'A' && bit_name[k] <= 'Z')
              bit_name[k] = static_cast<char>(bit_name[k] - 'A' + 'a');
          }
          auto *bs = new binary_sensor::BinarySensor();  // NOLINT: entities live for the whole runtime
          auto *bstored = new std::string(bit_name);     // NOLINT: same lifetime as the entity
          this->names_.push_back(bstored);
          const uint32_t bhash = fnv1_hash_object_id(bstored->c_str(), bstored->size());
          const uint32_t bfields = static_cast<uint32_t>(ENTITY_CATEGORY_DIAGNOSTIC)
                                   << ENTITY_FIELD_ENTITY_CATEGORY_SHIFT;
          App.register_binary_sensor(bs, bstored->c_str(), bhash, bfields);
          this->bit_bindings_.push_back(
              BitBinding{static_cast<uint16_t>(&b - &this->bindings_[0]), static_cast<uint8_t>(sym.value), bs});
          created++;
        }
      }
#endif
      continue;
    }
#ifdef USE_SENSOR
    // The Application containers silently drop pushes beyond their compile-time size.
    if (App.get_sensors().size() >= ESPHOME_ENTITY_SENSOR_COUNT) {
      skipped++;
      continue;
    }
    auto *sens = new sensor::Sensor();  // NOLINT: entities live for the whole runtime, never freed
    // The entity keeps a pointer to the name, so it must outlive the entity.
    auto *stored = new std::string(name);  // NOLINT: same lifetime as the entity
    this->names_.push_back(stored);
    const uint32_t hash = fnv1_hash_object_id(stored->c_str(), stored->size());
    const uint8_t dc = this->device_class_indices_[static_cast<size_t>(b.def->kind) < POINT_KIND_COUNT
                                                       ? static_cast<size_t>(b.def->kind)
                                                       : 0];
    const uint32_t fields = (static_cast<uint32_t>(dc) << ENTITY_FIELD_DC_SHIFT) |
                            (static_cast<uint32_t>(this->unit_index_(b.def->unit)) << ENTITY_FIELD_UOM_SHIFT);
    sens->set_accuracy_decimals(b.decimals);
    const bool accumulator = b.def->type == PointType::ACC32 || b.def->type == PointType::ACC64;
    sens->set_state_class(accumulator ? sensor::STATE_CLASS_TOTAL_INCREASING : sensor::STATE_CLASS_MEASUREMENT);
    App.register_sensor(sens, stored->c_str(), hash, fields);
    b.sensor = sens;
    created++;
#endif
  }
  this->entities_created_ = true;
  ESP_LOGI(TAG, "Created %u entities from %u model(s)", static_cast<unsigned>(created),
           static_cast<unsigned>(this->plan_.size()));
  if (skipped > 0)
    ESP_LOGE(TAG, "%u entities not created: no free slot (check 'models' and 'max_modules')",
             static_cast<unsigned>(skipped));
  // One single resync for the whole batch.
  if (created > 0)
    this->request_resync_();
}

#ifdef USE_BINARY_SENSOR
void SunSpecClient::publish_bits_(uint16_t binding_index, const Binding &b) {
  if (b.def->size == 0 || static_cast<size_t>(b.offset) + b.def->size > this->buffer_.size())
    return;
  uint32_t bits = this->buffer_[b.offset];
  if (b.def->type == PointType::BITFIELD32)
    bits = (bits << 16) | this->buffer_[b.offset + 1];
  for (auto &bb : this->bit_bindings_) {
    if (bb.binding_index == binding_index)
      bb.sensor->publish_state((bits >> bb.bit) & 1u);
  }
}
#endif

void SunSpecClient::publish_values_(uint8_t model_index) {
  for (auto &b : this->bindings_) {
    if (b.model_index != model_index)
      continue;
#ifdef USE_TEXT_SENSOR
    if (b.text_sensor != nullptr) {
      std::string text;
      // Keep the last text when the point is not readable, publish only changes
      if (decode_text(this->buffer_, *b.def, b.offset, text)) {
        if (!b.text_sensor->has_state() || b.text_sensor->state != text) {
          ESP_LOGV(TAG, "model %u offset %u '%s' = %s", this->models_[model_index].id, b.offset, b.def->key,
                   text.c_str());
          b.text_sensor->publish_state(text);
        }
#ifdef USE_BINARY_SENSOR
        this->publish_bits_(static_cast<uint16_t>(&b - &this->bindings_[0]), b);
#endif
      }
      continue;
    }
#endif
#ifdef USE_SENSOR
    if (b.sensor == nullptr)
      continue;
    float value;
    if (decode_point(this->buffer_, *b.def, b.offset, b.sf_offset, false, value)) {
      ESP_LOGV(TAG, "model %u offset %u '%s' = %.*f %s", this->models_[model_index].id, b.offset, b.def->key,
               b.decimals, value, b.def->unit);
      b.sensor->publish_state(value);
    } else
      b.sensor->publish_state(std::numeric_limits<float>::quiet_NaN());
#endif
  }
}

// ---------------------------------------------------------------------------------------------------------
// Home Assistant resync: HA only reads the entity list when it connects, so the API clients are asked to
// reconnect once the new entities are registered.

void SunSpecClient::request_resync_() {
  // Same timeout name: several requests within the delay collapse into one.
  this->set_timeout("api_resync", 500, [this]() { this->do_resync_(); });
}

void SunSpecClient::do_resync_() {
#ifdef USE_API
  if (api::global_api_server == nullptr || !api::global_api_server->is_connected()) {
    // No client: Home Assistant lists the entities when it connects.
    ESP_LOGD(TAG, "No API client connected, resync skipped");
    return;
  }
  ESP_LOGI(TAG, "Asking the API clients to reconnect to refresh the entity list");
  for (const auto &conn : api::global_api_server->active_clients()) {
    api::DisconnectRequest req;
    if (!conn->send_message(req))
      conn->on_fatal_error();  // TX buffer full: drop the socket, the client reconnects anyway
  }
#endif
}

}  // namespace esphome::sunspec_client
