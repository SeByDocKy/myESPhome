#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/camera/camera.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "usb/usb_host.h"
#include "usb/uvc_host.h"

#include <memory>
#include <vector>

namespace esphome {
namespace usbuvc {

// ---------------------------------------------------------------------------
// CameraImageData struct exposed to automations (on_image trigger)
// ---------------------------------------------------------------------------
struct UsbUvcCameraImageData {
  uint8_t *data{nullptr};
  size_t   length{0};
};

// ---------------------------------------------------------------------------
// UsbUvcImage: wraps a single UVC frame (malloc'd copy)
// ---------------------------------------------------------------------------
class UsbUvcImage : public camera::CameraImage {
 public:
  UsbUvcImage(uint8_t *data, size_t len, uint8_t requesters);
  ~UsbUvcImage() override;

  uint8_t *get_data_buffer() override { return this->data_; }
  size_t   get_data_length() override { return this->len_; }
  bool     was_requested_by(camera::CameraRequester requester) const override;

 protected:
  uint8_t *data_{nullptr};
  size_t   len_{0};
  uint8_t  requesters_{0};
};

// ---------------------------------------------------------------------------
// UsbUvcImageReader: sequential reader used by the API/native protocol layer
// ---------------------------------------------------------------------------
class UsbUvcImageReader : public camera::CameraImageReader {
 public:
  void     set_image(std::shared_ptr<camera::CameraImage> image) override;
  size_t   available() const override;
  uint8_t *peek_data_buffer() override;
  void     consume_data(size_t consumed) override;
  void     return_image() override;

 protected:
  std::shared_ptr<UsbUvcImage> image_;
  size_t offset_{0};
};

// ---------------------------------------------------------------------------
// Forward declaration
// ---------------------------------------------------------------------------
class UsbUvcCamera;

// ---------------------------------------------------------------------------
// Trigger types
// ---------------------------------------------------------------------------
class UsbUvcStreamStartTrigger : public Trigger<> {
 public:
  explicit UsbUvcStreamStartTrigger(UsbUvcCamera *parent);
};

class UsbUvcStreamStopTrigger : public Trigger<> {
 public:
  explicit UsbUvcStreamStopTrigger(UsbUvcCamera *parent);
};

class UsbUvcImageTrigger : public Trigger<UsbUvcCameraImageData> {
 public:
  explicit UsbUvcImageTrigger(UsbUvcCamera *parent);
};

// ---------------------------------------------------------------------------
// Main component
// camera::Camera already inherits from EntityBase and Component
// ---------------------------------------------------------------------------
class UsbUvcCamera : public camera::Camera {
 public:
  // ----- ESPHome lifecycle -------------------------------------------------
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // ----- camera::Camera interface ------------------------------------------
  void add_listener(camera::CameraListener *listener) override {
    this->listeners_.push_back(listener);
  }
  void start_stream(camera::CameraRequester requester) override;
  void stop_stream(camera::CameraRequester requester) override;
  void request_image(camera::CameraRequester requester) override;
  camera::CameraImageReader *create_image_reader() override;

  // ----- Setters called from Python codegen --------------------------------
  void set_vid(uint16_t vid)                 { this->vid_ = vid; }
  void set_pid(uint16_t pid)                 { this->pid_ = pid; }
  void set_uvc_stream_index(uint8_t idx)     { this->uvc_stream_index_ = idx; }
  void set_resolution(uint16_t w, uint16_t h){ this->width_ = w; this->height_ = h; }
  void set_fps(uint8_t fps)                  { this->fps_ = fps; }
  void set_max_update_interval(uint32_t ms)  { this->max_update_interval_ = ms; }
  void set_idle_update_interval(uint32_t ms) { this->idle_update_interval_ = ms; }
  void set_frame_buffer_count(uint8_t n)     { this->frame_buffer_count_ = n; }
  void set_urb_count(uint8_t n)              { this->urb_count_ = n; }
  void set_urb_size(uint32_t sz)             { this->urb_size_ = sz; }
  void set_frame_size_max(uint32_t sz)       { this->frame_size_max_ = sz; }

  // ----- Trigger registration ----------------------------------------------
  void add_stream_start_trigger(UsbUvcStreamStartTrigger *t) { this->stream_start_triggers_.push_back(t); }
  void add_stream_stop_trigger(UsbUvcStreamStopTrigger  *t)  { this->stream_stop_triggers_.push_back(t); }
  void add_image_trigger(UsbUvcImageTrigger *t)               { this->image_triggers_.push_back(t); }

  // ----- Internal callbacks (called from RTOS task context) ----------------
  bool on_frame_(const uvc_host_frame_t *frame);
  void on_stream_event_(const uvc_host_stream_event_data_t *event);

 protected:
  // --- Configuration -------------------------------------------------------
  uint16_t vid_{0};
  uint16_t pid_{0};
  uint8_t  uvc_stream_index_{0};
  uint16_t width_{640};
  uint16_t height_{480};
  uint8_t  fps_{15};
  uint32_t max_update_interval_{66};
  uint32_t idle_update_interval_{10000};
  uint8_t  frame_buffer_count_{3};
  uint8_t  urb_count_{3};
  uint32_t urb_size_{10240};
  uint32_t frame_size_max_{0};

  // --- Runtime state -------------------------------------------------------
  uvc_host_stream_hdl_t uvc_stream_{nullptr};
  bool dev_connected_{false};

  uint8_t single_requesters_{0};
  uint8_t stream_requesters_{0};

  uint32_t last_update_{0};
  uint32_t last_idle_request_{0};

  std::shared_ptr<UsbUvcImage> current_image_;

  // Pending frame queue (PendingFrame = malloc'd copy)
  struct PendingFrame {
    uint8_t *data;
    size_t   len;
  };
  QueueHandle_t frame_queue_{nullptr};
  SemaphoreHandle_t stream_mutex_{nullptr};

  // --- Camera listeners (API server etc.) ----------------------------------
  std::vector<camera::CameraListener *> listeners_;

  // --- RTOS task handles ---------------------------------------------------
  TaskHandle_t usb_lib_task_handle_{nullptr};
  TaskHandle_t uvc_task_handle_{nullptr};

  // --- Internal helpers ----------------------------------------------------
  bool has_requested_image_() const { return this->single_requesters_ || this->stream_requesters_; }
  bool can_return_image_() const    { return this->current_image_.use_count() == 1; }

  void dispatch_new_image_(PendingFrame &pf);
  void fire_stream_start_triggers_();
  void fire_stream_stop_triggers_();

  // --- Static RTOS task entry points ---------------------------------------
  static void usb_lib_task_(void *pv);
  static void uvc_connect_task_(void *pv);

  // --- Trigger lists -------------------------------------------------------
  std::vector<UsbUvcStreamStartTrigger *> stream_start_triggers_;
  std::vector<UsbUvcStreamStopTrigger  *> stream_stop_triggers_;
  std::vector<UsbUvcImageTrigger       *> image_triggers_;
};

}  // namespace usbuvc
}  // namespace esphome

#endif  // USE_ESP32
