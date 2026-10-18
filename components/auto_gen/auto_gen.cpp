#include "auto_gen.h"

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace auto_gen {

static const char *const TAG = "auto_gen";

void AutoGenComponent::setup() {
#ifdef USE_SENSOR
  // Create the first demo sensor after the configured delay, from the main loop.
  this->set_timeout("create_demo", this->delay_ms_, [this]() { this->create_next_demo_sensor_(); });
#else
  ESP_LOGE(TAG, "USE_SENSOR is not defined, nothing can be created");
  this->mark_failed();
#endif
}

void AutoGenComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "auto_gen:");
  ESP_LOGCONFIG(TAG, "  Delay: %" PRIu32 " ms", this->delay_ms_);
  ESP_LOGCONFIG(TAG, "  Sensors to create: %u (one every %" PRIu32 " ms)", this->count_, this->create_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Reserved sensor slots: %u", this->max_sensors_);
  ESP_LOGCONFIG(TAG, "  Demo sensor base name: %s", this->sensor_name_.c_str());
}

#ifdef USE_SENSOR
sensor::Sensor *AutoGenComponent::create_sensor(const std::string &name) {
  if (this->created_ >= this->max_sensors_) {
    ESP_LOGW(TAG, "No free sensor slot left (max_sensors: %u), '%s' not created", this->max_sensors_, name.c_str());
    return nullptr;
  }

  // The Application containers silently drop pushes beyond their compile-time size,
  // so check the real remaining capacity explicitly.
  if (App.get_sensors().size() >= ESPHOME_ENTITY_SENSOR_COUNT) {
    ESP_LOGE(TAG, "Application sensor container is full, '%s' not created", name.c_str());
    return nullptr;
  }

  auto *sens = new sensor::Sensor();  // NOLINT: entities live for the whole runtime, never freed
  // The entity keeps a pointer to the name, so it must outlive the entity.
  auto *stored_name = new std::string(name);  // NOLINT: same lifetime as the entity
  const uint32_t object_id_hash = fnv1_hash_object_id(stored_name->c_str(), stored_name->size());
  const uint32_t entity_fields = static_cast<uint32_t>(this->uom_index_) << ENTITY_FIELD_UOM_SHIFT;

  sens->set_accuracy_decimals(1);
  sens->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
  App.register_sensor(sens, stored_name->c_str(), object_id_hash, entity_fields);

  this->created_++;
  ESP_LOGI(TAG, "Registered sensor '%s' (%u/%u slots used)", name.c_str(), this->created_, this->max_sensors_);
  return sens;
}
#endif

void AutoGenComponent::create_next_demo_sensor_() {
#ifdef USE_SENSOR
  const size_t index = this->demo_sensors_.size();

  // A single sensor keeps the plain name, several get a _<n> suffix.
  std::string name = this->sensor_name_;
  if (this->count_ > 1)
    name += "_" + std::to_string(index + 1);

  sensor::Sensor *sens = this->create_sensor(name);
  if (sens == nullptr)
    return;
  this->demo_sensors_.push_back(sens);

  if (index == 0) {
    // Publish changing values so the effect is visible once Home Assistant sees the entities.
    // Sensor n publishes the shared counter plus n * 100 so that they are easy to tell apart.
    this->set_interval("demo_publish", 5000, [this]() {
      this->demo_value_ += 1.0f;
      for (size_t i = 0; i < this->demo_sensors_.size(); i++)
        this->demo_sensors_[i]->publish_state(this->demo_value_ + 100.0f * static_cast<float>(i));
    });
  }

  // Let automations (e.g. api_resync) know that the entity list changed.
  this->registered_callback_.call();

  // Schedule the next creation until the requested count is reached.
  if (this->demo_sensors_.size() < this->count_) {
    this->set_timeout("create_demo", this->create_interval_ms_, [this]() { this->create_next_demo_sensor_(); });
  }
#endif
}

}  // namespace auto_gen
}  // namespace esphome
