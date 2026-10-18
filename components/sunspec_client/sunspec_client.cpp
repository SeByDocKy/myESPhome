#include "sunspec_client.h"

#include <cinttypes>
#include <iterator>

#include "esphome/core/log.h"

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
    this->fail_scan_("request refused by the Modbus hub");
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
  this->fail_scan_("no answer from the device");
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

}  // namespace esphome::sunspec_client
