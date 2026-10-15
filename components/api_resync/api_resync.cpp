#include "api_resync.h"

#ifdef USE_API

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api_resync {

static const char *const TAG = "api_resync";

void ApiResyncComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "API resync:");
  ESP_LOGCONFIG(TAG, "  Debounce delay: %" PRIu32 " ms", this->delay_ms_);
  ESP_LOGCONFIG(TAG, "  Min interval: %" PRIu32 " ms", this->min_interval_ms_);
}

void ApiResyncComponent::request_resync() {
  // Same timeout name: a new request replaces the pending one (debounce)
  this->set_timeout("api_resync", this->delay_ms_, [this]() { this->do_resync_(); });
}

void ApiResyncComponent::do_resync_() {
  if (api::global_api_server == nullptr || !api::global_api_server->is_connected()) {
    // No client: nothing to refresh, Home Assistant will list entities when it connects
    ESP_LOGD(TAG, "No API client connected, resync skipped");
    return;
  }

  const uint32_t now = millis();
  if (this->has_resynced_) {
    const uint32_t elapsed = now - this->last_resync_ms_;
    if (elapsed < this->min_interval_ms_) {
      // Too early: postpone to the end of the interval instead of dropping the request
      const uint32_t wait = this->min_interval_ms_ - elapsed;
      ESP_LOGD(TAG, "Rate limited, resync postponed by %" PRIu32 " ms", wait);
      this->set_timeout("api_resync", wait, [this]() { this->do_resync_(); });
      return;
    }
  }

  this->has_resynced_ = true;
  this->last_resync_ms_ = now;
  ESP_LOGD(TAG, "Asking API clients to reconnect");
  this->send_disconnect_requests_();
}

void ApiResyncComponent::send_disconnect_requests_() {
  for (const auto &conn : api::global_api_server->active_clients()) {
    api::DisconnectRequest req;  // reason defaults to UNSPECIFIED
    if (!conn->send_message(req))
      conn->on_fatal_error();  // TX buffer full: drop the socket, the client reconnects anyway
  }
}

}  // namespace api_resync
}  // namespace esphome

#endif  // USE_API
