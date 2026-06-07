#ifdef USE_ESP32

#include "usbuvc.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

#include <cstring>
#include <malloc.h>

namespace esphome {
namespace usbuvc {

static const char *const TAG = "usbuvc";

// Priority / stack sizes – tuned for ESP32-S3
static constexpr uint32_t USB_LIB_TASK_STACK   = 4096;
static constexpr uint32_t UVC_CONNECT_TASK_STACK = 4096;
static constexpr UBaseType_t USB_LIB_TASK_PRIO  = 15;
static constexpr UBaseType_t UVC_TASK_PRIO       = 14;

// How many raw frame pointers to buffer before dropping
static constexpr uint8_t FRAME_QUEUE_DEPTH = 4;

UsbUvcCamera *global_usb_uvc_camera = nullptr;  // NOLINT

// ===========================================================================
// Static UVC callbacks  (called from UVC driver task context)
// ===========================================================================

static bool uvc_frame_callback(const uvc_host_frame_t *frame, void *user_ctx) {
  // user_ctx is ignored – we use the global singleton
  if (global_usb_uvc_camera == nullptr || frame == nullptr)
    return true;  // return frame immediately

  return global_usb_uvc_camera->on_frame_(frame);
}

static void uvc_stream_callback(const uvc_host_stream_event_data_t *event, void *user_ctx) {
  if (global_usb_uvc_camera == nullptr || event == nullptr)
    return;
  global_usb_uvc_camera->on_stream_event_(event);
}

// ===========================================================================
// UsbUvcImage
// ===========================================================================

UsbUvcImage::UsbUvcImage(uint8_t *data, size_t len, uint8_t requesters)
    : data_(data), len_(len), requesters_(requesters) {}

UsbUvcImage::~UsbUvcImage() {
  if (this->data_ != nullptr) {
    free(this->data_);  // NOLINT(cppcoreguidelines-no-malloc)
    this->data_ = nullptr;
  }
}

bool UsbUvcImage::was_requested_by(camera::CameraRequester requester) const {
  return (this->requesters_ & (1U << static_cast<uint8_t>(requester))) != 0;
}

// ===========================================================================
// UsbUvcImageReader
// ===========================================================================

void UsbUvcImageReader::set_image(std::shared_ptr<camera::CameraImage> image) {
  this->image_  = std::static_pointer_cast<UsbUvcImage>(image);
  this->offset_ = 0;
}

size_t UsbUvcImageReader::available() const {
  if (!this->image_)
    return 0;
  return this->image_->get_data_length() - this->offset_;
}

uint8_t *UsbUvcImageReader::peek_data_buffer() {
  return this->image_->get_data_buffer() + this->offset_;
}

void UsbUvcImageReader::consume_data(size_t consumed) { this->offset_ += consumed; }

void UsbUvcImageReader::return_image() { this->image_.reset(); }

// ===========================================================================
// Trigger constructors
// ===========================================================================

UsbUvcStreamStartTrigger::UsbUvcStreamStartTrigger(UsbUvcCamera *parent) {
  parent->add_stream_start_trigger(this);
}

UsbUvcStreamStopTrigger::UsbUvcStreamStopTrigger(UsbUvcCamera *parent) {
  parent->add_stream_stop_trigger(this);
}

UsbUvcImageTrigger::UsbUvcImageTrigger(UsbUvcCamera *parent) {
  parent->add_image_trigger(this);
}

// ===========================================================================
// UsbUvcCamera – ESPHome lifecycle
// ===========================================================================

void UsbUvcCamera::setup() {
  global_usb_uvc_camera = this;
  this->last_update_      = millis();
  this->last_idle_request_ = millis();

  // Frame queue: main loop picks up PendingFrame structs from here
  this->frame_queue_  = xQueueCreate(FRAME_QUEUE_DEPTH, sizeof(PendingFrame));
  this->stream_mutex_ = xSemaphoreCreateMutex();

  if (this->frame_queue_ == nullptr || this->stream_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate RTOS primitives");
    this->mark_failed();
    return;
  }

  // --- Install USB host driver (must be done once per firmware) ------------
  const usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .intr_flags     = ESP_INTR_FLAG_LOWMED,
  };
  esp_err_t err = usb_host_install(&host_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_install failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // --- USB library event task ----------------------------------------------
  BaseType_t created = xTaskCreatePinnedToCore(
      UsbUvcCamera::usb_lib_task_,
      "usbuvc_lib",
      USB_LIB_TASK_STACK,
      nullptr,
      USB_LIB_TASK_PRIO,
      &this->usb_lib_task_handle_,
      tskNO_AFFINITY);
  if (created != pdTRUE) {
    ESP_LOGE(TAG, "Failed to create USB lib task");
    this->mark_failed();
    return;
  }

  // --- Install UVC host driver ---------------------------------------------
  const uvc_host_driver_config_t uvc_driver_config = {
      .driver_task_stack_size   = 4096,
      .driver_task_priority     = USB_LIB_TASK_PRIO + 1,
      .xCoreID                  = tskNO_AFFINITY,
      .create_background_task   = true,
  };
  err = uvc_host_install(&uvc_driver_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uvc_host_install failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // --- UVC connect / stream task -------------------------------------------
  created = xTaskCreatePinnedToCore(
      UsbUvcCamera::uvc_connect_task_,
      "usbuvc_conn",
      UVC_CONNECT_TASK_STACK,
      this,
      UVC_TASK_PRIO,
      &this->uvc_task_handle_,
      tskNO_AFFINITY);
  if (created != pdTRUE) {
    ESP_LOGE(TAG, "Failed to create UVC connect task");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "USB UVC camera setup OK");
}

// ---------------------------------------------------------------------------
void UsbUvcCamera::dump_config() {
  ESP_LOGCONFIG(TAG,
                "USB UVC Camera:\n"
                "  Name: %s\n"
                "  USB VID: 0x%04X  PID: 0x%04X  StreamIdx: %u\n"
                "  Resolution: %ux%u @ %u fps\n"
                "  Max framerate interval: %u ms\n"
                "  Idle framerate interval: %u ms\n"
                "  Frame buffers: %u  URBs: %u x %u B",
                this->get_name().c_str(),
                this->vid_, this->pid_, this->uvc_stream_index_,
                this->width_, this->height_, this->fps_,
                this->max_update_interval_,
                this->idle_update_interval_,
                this->frame_buffer_count_,
                this->urb_count_, this->urb_size_);
  if (this->is_failed())
    ESP_LOGE(TAG, "  Setup FAILED");
}

// ---------------------------------------------------------------------------
void UsbUvcCamera::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // ---- Idle heartbeat: keep delivering frames even when nobody is watching
  if (!this->has_requested_image_()) {
    if (this->idle_update_interval_ != 0 &&
        now - this->last_idle_request_ > this->idle_update_interval_) {
      this->last_idle_request_ = now;
      this->request_image(camera::IDLE);
    } else {
      return;
    }
  }

  // ---- Release current image if all consumers are done
  if (this->can_return_image_()) {
    this->current_image_.reset();
  }

  if (!this->has_requested_image_())
    return;

  if (this->current_image_ && this->current_image_.use_count() > 1) {
    // Still in use by a listener / reader
    return;
  }

  // Rate-limiting
  if (now - this->last_update_ < this->max_update_interval_)
    return;

  // ---- Try to pull a frame from the queue
  PendingFrame pf{};
  if (xQueueReceive(this->frame_queue_, &pf, 0) != pdTRUE) {
    return;  // nothing ready yet
  }

  if (pf.data == nullptr || pf.len == 0) {
    free(pf.data);  // NOLINT
    return;
  }

  this->dispatch_new_image_(pf);
}

// ===========================================================================
// camera::Camera interface
// ===========================================================================

void UsbUvcCamera::start_stream(camera::CameraRequester requester) {
  this->stream_requesters_ |= (1U << static_cast<uint8_t>(requester));
  this->fire_stream_start_triggers_();
}

void UsbUvcCamera::stop_stream(camera::CameraRequester requester) {
  this->stream_requesters_ &= ~(1U << static_cast<uint8_t>(requester));
  this->fire_stream_stop_triggers_();
}

void UsbUvcCamera::request_image(camera::CameraRequester requester) {
  this->single_requesters_ |= (1U << static_cast<uint8_t>(requester));
}

camera::CameraImageReader *UsbUvcCamera::create_image_reader() {
  return new UsbUvcImageReader();
}

// ===========================================================================
// UVC callbacks  (called from UVC driver RTOS task)
// ===========================================================================

bool UsbUvcCamera::on_frame_(const uvc_host_frame_t *frame) {
  // We must return the frame to the driver quickly – copy the payload and
  // enqueue a PendingFrame for the main ESPHome loop.
  if (frame->data_len == 0)
    return true;  // nothing useful, return immediately

  // Only copy if someone actually wants a frame
  if (!this->has_requested_image_()) {
    return true;
  }

  uint8_t *copy = static_cast<uint8_t *>(malloc(frame->data_len));  // NOLINT
  if (copy == nullptr) {
    ESP_LOGW(TAG, "OOM: dropping frame of %u bytes", frame->data_len);
    return true;
  }
  memcpy(copy, frame->data, frame->data_len);

  PendingFrame pf{copy, frame->data_len};
  if (xQueueSendToBack(this->frame_queue_, &pf, 0) != pdPASS) {
    ESP_LOGV(TAG, "Frame queue full – dropping frame");
    free(copy);  // NOLINT
    return true;
  }

  // Wake main loop so it picks the frame up promptly
  App.wake_loop_threadsafe();

  // Return true = we are done with the frame (driver can reclaim it)
  return true;
}

void UsbUvcCamera::on_stream_event_(const uvc_host_stream_event_data_t *event) {
  switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
      ESP_LOGE(TAG, "USB transfer error: %d", event->transfer_error.error);
      break;

    case UVC_HOST_DEVICE_DISCONNECTED:
      ESP_LOGI(TAG, "UVC device disconnected");
      this->dev_connected_ = false;
      xSemaphoreTake(this->stream_mutex_, portMAX_DELAY);
      if (this->uvc_stream_ != nullptr) {
        uvc_host_stream_close(event->device_disconnected.stream_hdl);
        this->uvc_stream_ = nullptr;
      }
      xSemaphoreGive(this->stream_mutex_);
      this->fire_stream_stop_triggers_();
      break;

    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
      ESP_LOGW(TAG, "UVC frame buffer overflow");
      break;

    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
      ESP_LOGW(TAG, "UVC frame buffer underflow");
      break;

    default:
      ESP_LOGV(TAG, "UVC event: %d", event->type);
      break;
  }
}

// ===========================================================================
// Internal helpers
// ===========================================================================

void UsbUvcCamera::dispatch_new_image_(PendingFrame &pf) {
  // Build a shared UsbUvcImage that takes ownership of pf.data
  this->current_image_ = std::make_shared<UsbUvcImage>(
      pf.data, pf.len,
      this->single_requesters_ | this->stream_requesters_);

  ESP_LOGV(TAG, "New frame: %u bytes", pf.len);

  // Notify all camera listeners (API server, automations, …)
  for (auto *listener : this->listeners_) {
    listener->on_camera_image(this->current_image_);
  }

  // Fire on_image triggers
  UsbUvcCameraImageData img_data;
  img_data.data   = pf.data;
  img_data.length = pf.len;
  for (auto *t : this->image_triggers_)
    t->trigger(img_data);

  this->last_update_      = millis();
  this->single_requesters_ = 0;
}

void UsbUvcCamera::fire_stream_start_triggers_() {
  for (auto *t : this->stream_start_triggers_)
    t->trigger();
}

void UsbUvcCamera::fire_stream_stop_triggers_() {
  for (auto *t : this->stream_stop_triggers_)
    t->trigger();
}

// ===========================================================================
// RTOS tasks
// ===========================================================================

void UsbUvcCamera::usb_lib_task_(void * /*pv*/) {
  while (true) {
    uint32_t event_flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
      usb_host_device_free_all();

    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
      ESP_LOGD(TAG, "USB: all devices freed");
  }
}

void UsbUvcCamera::uvc_connect_task_(void *pv) {
  UsbUvcCamera *self = static_cast<UsbUvcCamera *>(pv);

  // Build stream config from component settings
  uvc_host_stream_config_t stream_config = {};
  stream_config.event_cb  = uvc_stream_callback;
  stream_config.frame_cb  = uvc_frame_callback;
  stream_config.user_ctx  = nullptr;  // we use the global singleton

  stream_config.usb.vid             = self->vid_;
  stream_config.usb.pid             = self->pid_;
  stream_config.usb.uvc_stream_index = self->uvc_stream_index_;

  stream_config.vs_format.h_res  = self->width_;
  stream_config.vs_format.v_res  = self->height_;
  stream_config.vs_format.fps    = static_cast<float>(self->fps_);
  stream_config.vs_format.format = UVC_VS_FORMAT_MJPEG;

  stream_config.advanced.frame_size           = self->frame_size_max_;
  stream_config.advanced.number_of_frame_buffers = self->frame_buffer_count_;
  stream_config.advanced.number_of_urbs       = self->urb_count_;
  stream_config.advanced.urb_size             = self->urb_size_;
  stream_config.advanced.frame_heap_caps      = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

  while (true) {
    ESP_LOGI(TAG, "Trying to open UVC device 0x%04X:0x%04X %ux%u@%.0ffps …",
             stream_config.usb.vid, stream_config.usb.pid,
             stream_config.vs_format.h_res, stream_config.vs_format.v_res,
             stream_config.vs_format.fps);

    uvc_host_stream_hdl_t stream_hdl = nullptr;
    esp_err_t err = uvc_host_stream_open(&stream_config, pdMS_TO_TICKS(5000), &stream_hdl);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "uvc_host_stream_open failed (%s), retrying in 5 s …",
               esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    xSemaphoreTake(self->stream_mutex_, portMAX_DELAY);
    self->uvc_stream_    = stream_hdl;
    self->dev_connected_ = true;
    xSemaphoreGive(self->stream_mutex_);

    ESP_LOGI(TAG, "UVC device opened – starting stream");
    self->fire_stream_start_triggers_();

    uvc_host_stream_start(stream_hdl);

    // Wait here until the device disconnects (on_stream_event_ clears dev_connected_)
    while (self->dev_connected_) {
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "Device gone – waiting before reconnect …");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

}  // namespace usbuvc
}  // namespace esphome

#endif  // USE_ESP32
