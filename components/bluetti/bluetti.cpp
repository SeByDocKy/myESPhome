#include "bluetti.h"

namespace esphome::bluetti {

static const char *const TAG = "bluetti";

void BluettiHub::setup() {
  if (this->controller_ == nullptr) {
    ESP_LOGE(TAG, "No modbus_controller: nothing to talk to the device with");
    this->mark_failed();
    return;
  }
  const bool has_slow = this->slow_controller_ != nullptr && this->slow_controller_ != this->controller_;
  if (!has_slow) {
    // One controller polls everything
    this->fast_items_.insert(this->fast_items_.end(), this->slow_items_.begin(), this->slow_items_.end());
    this->slow_items_.clear();
  }

  // The hub adds its own reader here, before the controllers build their read ranges from the registered items
  // (this hub's setup priority is higher than theirs): the first register of the device, read at every poll, tells
  // that the device answers (the controller only reports the changes).
  if (!this->blocks_.empty()) {
    this->heartbeat_reader_.configure(this->blocks_.front().first, 1, SensorValueType::U_WORD,
                                      EntityType::HOLDING);
    this->heartbeat_reader_.set_callback([this](int64_t) { this->set_connected_(true); });
    this->fast_items_.push_back(&this->heartbeat_reader_);
  }

  this->register_items_(this->fast_items_, this->controller_);
  if (has_slow)
    this->register_items_(this->slow_items_, this->slow_controller_);

  this->controller_->add_on_online_callback([this](int, int) { this->set_connected_(true); });
  this->controller_->add_on_offline_callback([this](int, int) { this->set_connected_(false); });
  this->set_connected_(false);
}

void BluettiHub::add_item(SensorItem *item, bool slow) { (slow ? this->slow_items_ : this->fast_items_).push_back(item); }

int BluettiHub::block_of_(uint16_t start, uint16_t count) const {
  for (size_t i = 0; i < this->blocks_.size(); i++) {
    if (start >= this->blocks_[i].first && static_cast<uint32_t>(start) + count - 1 <= this->blocks_[i].second)
      return static_cast<int>(i);
  }
  return -1;
}

void BluettiHub::register_items_(std::vector<SensorItem *> &items, ModbusController *controller) {
  std::stable_sort(items.begin(), items.end(),
                   [](const SensorItem *a, const SensorItem *b) { return a->start_address < b->start_address; });

  uint16_t ranges = 0;
  bool have_chunk = false;
  int chunk_block = -1;
  uint16_t chunk_first = 0;  // first register of the current read
  uint32_t chunk_end = 0;    // one past the last register of the current read
  for (SensorItem *item : items) {
    const uint16_t start = item->start_address;
    const uint16_t count = item->entity_count();
    const uint32_t end = static_cast<uint32_t>(start) + count;
    const int block = this->block_of_(start, count);
    if (block < 0)
      ESP_LOGW(TAG, "Register %u (%u registers) is outside the register blocks of the %s", start, count, this->model_);

    // The same address as the first register of the current read is always part of that read
    bool new_read = !have_chunk;
    if (have_chunk && start != chunk_first) {
      new_read = block < 0 || block != chunk_block || start > chunk_end || end - chunk_first > this->max_span_;
    }
    if (new_read) {
      chunk_first = start;
      chunk_block = block;
      chunk_end = end;
      have_chunk = true;
      ranges++;
    } else if (end > chunk_end) {
      chunk_end = end;
    }
    // A read starts at an item that must not join the previous range; the others follow it
    item->reuse_previous_range = (start == chunk_first) ? RangeReuse::NEVER : RangeReuse::AUTO;
    controller->add_sensor_item(item);
  }
  (controller == this->controller_ ? this->fast_ranges_ : this->slow_ranges_) = ranges;
}

void BluettiHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BLUETTI:\n"
                "  Model: %s\n"
                "  Max registers per read: %u\n"
                "  Register blocks: %u\n"
                "  Reads per poll: %u (fast), %u (slow)\n"
                "  Slow controller: %s",
                this->model_, static_cast<unsigned>(this->max_span_), static_cast<unsigned>(this->blocks_.size()),
                static_cast<unsigned>(this->fast_ranges_), static_cast<unsigned>(this->slow_ranges_),
                YESNO(this->slow_controller_ != nullptr && this->slow_controller_ != this->controller_));
}

void BluettiHub::set_connected_(bool connected) {
  if (this->connected_known_ && this->connected_ == connected)
    return;
  this->connected_known_ = true;
  this->connected_ = connected;
  if (connected) {
    ESP_LOGI(TAG, "Device answers (%s)", this->model_);
  }
#ifdef USE_BINARY_SENSOR
  if (this->connection_sensor_ != nullptr)
    this->connection_sensor_->publish_state(connected);
#endif
}

}  // namespace esphome::bluetti
