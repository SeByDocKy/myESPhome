#include "marstek_text_sensor.h"

namespace esphome::marstek {

static const char *const TAG = "marstek.text_sensor";

void MarstekTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Marstek Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u  skip_updates: %u", this->reg_, this->register_count,
                this->skip_updates);
}

void MarstekTextSensor::publish_if_changed_(const std::string &value) {
  if (this->published_ && value == this->last_)
    return;
  this->last_ = value;
  this->published_ = true;
  this->publish_state(value);
}

// Raw bytes of the registers of this item; when `stop_at_nul` the string ends at the first NUL byte
std::string MarstekTextSensor::decode_bytes_(const std::vector<uint8_t> &data, bool stop_at_nul) const {
  std::string out;
  const size_t length = static_cast<size_t>(this->register_count) * 2u;
  out.reserve(length);
  for (size_t i = 0; i < length; i++) {
    const char c = static_cast<char>(data[this->offset + i]);
    if (stop_at_nul && c == '\0')
      break;
    out.push_back(c);
  }
  return out;
}

void MarstekTextSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data))
    return;

  switch (this->kind_) {
    case TextKind::CHAR: {
      std::string raw = this->decode_bytes_(data, true);
      std::string text;
      for (char c : raw) {
        if (c >= 0x20 && c <= 0x7E)  // printable ASCII only
          text.push_back(c);
      }
      while (!text.empty() && text.back() == ' ')
        text.pop_back();
      this->publish_if_changed_(text);
      break;
    }
    case TextKind::MAC: {
      const std::string raw = this->decode_bytes_(data, true);
      bool ascii_hex = raw.size() == 12;
      for (char c : raw) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
          ascii_hex = false;
      }
      std::string mac;
      char buf[4];
      if (ascii_hex) {
        // "009B0805D90A" -> "00:9B:08:05:D9:0A"
        for (size_t i = 0; i < 12; i += 2) {
          if (i != 0)
            mac.push_back(':');
          mac.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(raw[i]))));
          mac.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(raw[i + 1]))));
        }
      } else {
        // Not ASCII hex: show the bytes as they are
        for (size_t i = 0; i < raw.size(); i++) {
          if (i != 0)
            mac.push_back(':');
          snprintf(buf, sizeof(buf), "%02X", static_cast<uint8_t>(raw[i]));
          mac += buf;
        }
      }
      this->publish_if_changed_(mac);
      break;
    }
    case TextKind::IPV4: {
      // Bytes in register order: a.b.c.d (not confirmed on hardware, see the README)
      char buf[20];
      snprintf(buf, sizeof(buf), "%u.%u.%u.%u", data[this->offset], data[this->offset + 1], data[this->offset + 2],
               data[this->offset + 3]);
      this->publish_if_changed_(buf);
      break;
    }
    case TextKind::STATES: {
      const uint16_t value = this->word_(data, 0);
      std::string label;
      for (const auto &state : this->states_) {
        if (state.first == value) {
          label = state.second;
          break;
        }
      }
      if (label.empty()) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Unknown (%u)", value);
        label = buf;
      }
      this->publish_if_changed_(label);
      break;
    }
    case TextKind::BITS: {
      std::string active;
      const size_t bit_count = static_cast<size_t>(this->register_count) * 16u;
      for (size_t bit = 0; bit < bit_count; bit++) {
        if (((this->word_(data, bit / 16) >> (bit % 16)) & 1u) == 0)
          continue;
        std::string name;
        for (const auto &entry : this->bits_) {
          if (entry.first == bit) {
            name = entry.second;
            break;
          }
        }
        if (name.empty()) {
          char buf[16];
          snprintf(buf, sizeof(buf), "Bit %u", static_cast<unsigned>(bit));
          name = buf;
        }
        if (!active.empty())
          active += ", ";
        active += name;
      }
      this->publish_if_changed_(active.empty() ? std::string("OK") : active);
      break;
    }
  }
}

void MarstekFirmwareTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Marstek Firmware Version", this);
  ESP_LOGCONFIG(TAG, "  Parts: EMS%s + BMS", this->with_vms_ ? " + VMS" : "");
}

void MarstekFirmwareTextSensor::configure_dep(uint8_t index, uint16_t reg, uint8_t count, SensorValueType vtype,
                                              float scale, uint16_t skip_updates) {
  if (index >= 3)
    return;
  this->deps_[index].configure(reg, count, vtype, skip_updates);
  this->deps_[index].set_scale(scale);
  this->deps_[index].set_callback([this]() { this->recompute_(); });
}

void MarstekFirmwareTextSensor::recompute_() {
  if (!this->deps_[0].valid() || !this->deps_[1].valid() || (this->with_vms_ && !this->deps_[2].valid()))
    return;

  const int ems = static_cast<int>(this->deps_[0].value());
  const int bms = static_cast<int>(this->deps_[1].value());

  // EMS: a 4 digit value carries a tenth in its last digit (1476 -> 147.6), 3 digits are whole
  char ems_str[16];
  if (ems >= 1000) {
    snprintf(ems_str, sizeof(ems_str), "%d.%d", ems / 10, ems % 10);
  } else {
    snprintf(ems_str, sizeof(ems_str), "%d", ems);
  }

  char version[48];
  if (this->with_vms_) {
    snprintf(version, sizeof(version), "V%s.%d.%d", ems_str, static_cast<int>(this->deps_[2].value()), bms);
  } else {
    snprintf(version, sizeof(version), "V%s.%d", ems_str, bms);
  }

  if (this->published_ && this->last_ == version)
    return;
  this->last_ = version;
  this->published_ = true;
  this->publish_state(this->last_);
}

}  // namespace esphome::marstek
