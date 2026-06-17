#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/camera/camera.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

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
// Struct exposé aux automations on_image
// ---------------------------------------------------------------------------
struct UsbUvcCameraImageData {
  uint8_t *data{nullptr};
  size_t   length{0};
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
class UsbUvcCamera;

#ifdef USE_TEXT_SENSOR
// ---------------------------------------------------------------------------
// TextSensor listant les formats MJPEG disponibles
// Déclaré AVANT UsbUvcCamera pour pouvoir être utilisé dans set_format_list_sensor()
// ---------------------------------------------------------------------------
class UvcFormatListSensor : public text_sensor::TextSensor, public Component {
 public:
  void setup() override {}
};
#endif  // USE_TEXT_SENSOR

#ifdef USE_SELECT
// ---------------------------------------------------------------------------
// Select ESPHome qui liste les résolutions MJPEG disponibles.
// Peuplé automatiquement lors de la connexion UVC.
// Appelle action_change_format() à chaque changement de valeur.
// ---------------------------------------------------------------------------
class UsbUvcCamera;  // forward pour UvcResolutionSelect

class UvcResolutionSelect : public select::Select, public Component {
 public:
  void setup() override {}
  void set_parent(UsbUvcCamera *parent) { this->parent_ = parent; }

 protected:
  void control(const std::string &value) override;
  UsbUvcCamera *parent_{nullptr};

  // Stockage des labels (FixedVector stocke const char* → durée de vie garantie ici)
  std::vector<std::string> option_strings_;

 public:
  // Appelé par publish_format_list_ pour reconstruire les options dynamiquement
  void update_options(const std::vector<std::string> &options);
};
#endif  // USE_SELECT

#ifdef USE_NUMBER
// ---------------------------------------------------------------------------
// Number (slider) pour controler le downsampling_factor a la volee.
// Declare AVANT UsbUvcCamera pour etre utilise dans set_downsampling_number()
// ---------------------------------------------------------------------------
class UsbUvcCamera;  // forward pour UvcDownsamplingNumber

class UvcDownsamplingNumber : public number::Number, public Component {
 public:
  void set_parent(UsbUvcCamera *parent) { this->parent_ = parent; }
  void setup() override;

 protected:
  void control(float value) override;
  UsbUvcCamera *parent_{nullptr};
};
#endif  // USE_NUMBER

// ---------------------------------------------------------------------------
// UsbUvcImage : encapsule un frame UVC (copie malloc'd, owned)
// ---------------------------------------------------------------------------
class UsbUvcImage : public camera::CameraImage {
 public:
  UsbUvcImage(uint8_t *data, size_t len, uint8_t requesters);
  ~UsbUvcImage() override;

  uint8_t *get_data_buffer() override { return this->data_; }
  size_t   get_data_length()  override { return this->len_; }
  bool     was_requested_by(camera::CameraRequester requester) const override;

 protected:
  uint8_t *data_{nullptr};
  size_t   len_{0};
  uint8_t  requesters_{0};
};

// ---------------------------------------------------------------------------
// UsbUvcImageReader : lecteur séquentiel utilisé par le serveur API natif
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
// Triggers YAML
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
// Composant principal
// ---------------------------------------------------------------------------
class UsbUvcCamera : public camera::Camera {
 public:
  // ----- Cycle de vie ESPHome ---------------------------------------------
  void  setup() override;
  void  loop()  override;
  void  dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // ----- Interface camera::Camera -----------------------------------------
  void add_listener(camera::CameraListener *listener) override {
    this->listeners_.push_back(listener);
  }
  camera::CameraImageReader *create_image_reader() override;
  void request_image(camera::CameraRequester requester) override;
  void start_stream(camera::CameraRequester requester) override;
  void stop_stream(camera::CameraRequester requester)  override;

  // ----- Setters codegen --------------------------------------------------
  void set_vid(uint16_t vid)                  { this->vid_ = vid; }
  void set_pid(uint16_t pid)                  { this->pid_ = pid; }
  void set_uvc_stream_index(uint8_t idx)      { this->uvc_stream_index_ = idx; }
  void set_resolution(uint16_t w, uint16_t h) { this->width_ = w; this->height_ = h; }
  void set_fps(uint8_t fps)                   { this->fps_ = fps; }
  void set_max_update_interval(uint32_t ms)   { this->max_update_interval_ = ms; }
  void set_idle_update_interval(uint32_t ms)  { this->idle_update_interval_ = ms; }
  void set_frame_buffer_count(uint8_t n)      { this->frame_buffer_count_ = n; }
  void set_urb_count(uint8_t n)               { this->urb_count_ = n; }
  void set_urb_size(uint32_t sz)              { this->urb_size_ = sz; }
  void set_frame_size_max(uint32_t sz)        { this->frame_size_max_ = sz; }
  void set_start_streaming_at_init(bool v)    { this->start_streaming_at_init_ = v; }
  void set_downsampling_factor(uint8_t f)     { this->downsampling_factor_ = (f < 1) ? 1 : f; }
  uint8_t get_downsampling_factor() const     { return this->downsampling_factor_; }
#ifdef USE_NUMBER
  void set_downsampling_number(UvcDownsamplingNumber *n) { this->downsampling_number_ = n; if (n) n->set_parent(this); }
#endif

#ifdef USE_TEXT_SENSOR
  void set_format_list_sensor(UvcFormatListSensor *s) { this->format_list_sensor_ = s; }
#endif
#ifdef USE_SELECT
  void set_resolution_select(UvcResolutionSelect *s)  { this->resolution_select_ = s; if (s) s->set_parent(this); }
#endif

  // ----- Actions YAML -----------------------------------------------------
  void action_start_stream();
  void action_stop_stream();
  void action_change_format(const std::string &format);

  // ----- Enregistrement triggers ------------------------------------------
  void add_stream_start_trigger(UsbUvcStreamStartTrigger *t) { this->stream_start_triggers_.push_back(t); }
  void add_stream_stop_trigger (UsbUvcStreamStopTrigger  *t) { this->stream_stop_triggers_.push_back(t); }
  void add_image_trigger       (UsbUvcImageTrigger       *t) { this->image_triggers_.push_back(t); }

  // ----- Callbacks RTOS ---------------------------------------------------
  bool on_frame_(const uvc_host_frame_t *frame);
  void on_stream_event_(const uvc_host_stream_event_data_t *event);
  void on_driver_event_(const uvc_host_driver_event_data_t *event);

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
  bool start_streaming_at_init_{true};
  uint8_t downsampling_factor_{1};
  uint8_t downsampling_counter_{0};
#ifdef USE_NUMBER
  UvcDownsamplingNumber *downsampling_number_{nullptr};
#endif

  // --- Runtime -------------------------------------------------------------
  uvc_host_stream_hdl_t uvc_stream_{nullptr};
  bool dev_connected_{false};
  uint8_t dev_addr_{0};
  uint8_t connected_stream_index_{0};

  uint8_t single_requesters_{0};
  uint8_t stream_requesters_{0};

  uint32_t last_update_{0};
  uint32_t last_idle_request_{0};

  std::shared_ptr<UsbUvcImage> current_image_;

  struct PendingFrame {
    uint8_t *data;
    size_t   len;
  };
  QueueHandle_t     frame_queue_{nullptr};
  SemaphoreHandle_t stream_mutex_{nullptr};

  // --- Entités liées -------------------------------------------------------
#ifdef USE_TEXT_SENSOR
  UvcFormatListSensor *format_list_sensor_{nullptr};
#endif
#ifdef USE_SELECT
  UvcResolutionSelect *resolution_select_{nullptr};
#endif
  std::vector<camera::CameraListener *> listeners_;

  // --- Tâches RTOS ---------------------------------------------------------
  TaskHandle_t usb_lib_task_handle_{nullptr};
  TaskHandle_t uvc_task_handle_{nullptr};

  // --- Helpers internes ----------------------------------------------------
  bool has_requested_image_() const {
    return this->single_requesters_ != 0 || this->stream_requesters_ != 0;
  }
  bool can_release_image_() const {
    return !this->current_image_ || this->current_image_.use_count() == 1;
  }

  void dispatch_new_image_(PendingFrame &pf);
  void publish_format_list_(uint8_t dev_addr, uint8_t uvc_stream_index, size_t frame_info_num);
  void fire_stream_start_triggers_();
  void fire_stream_stop_triggers_();

  static void usb_lib_task_(void *pv);
  static void uvc_connect_task_(void *pv);

  // --- Listes triggers -----------------------------------------------------
  std::vector<UsbUvcStreamStartTrigger *> stream_start_triggers_;
  std::vector<UsbUvcStreamStopTrigger  *> stream_stop_triggers_;
  std::vector<UsbUvcImageTrigger       *> image_triggers_;
};

// ---------------------------------------------------------------------------
// Action usbuvc.change_format
// ---------------------------------------------------------------------------
template<typename... Ts>
class UsbUvcChangeFormatAction : public Action<Ts...>, public Parented<UsbUvcCamera> {
 public:
  TEMPLATABLE_VALUE(std::string, format)
  void play(const Ts &...x) override {
    this->parent_->action_change_format(this->format_.value(x...));
  }
};

// ---------------------------------------------------------------------------
// Actions usbuvc.start_stream / usbuvc.stop_stream
// ---------------------------------------------------------------------------
template<typename... Ts>
class UsbUvcStartStreamAction : public Action<Ts...>, public Parented<UsbUvcCamera> {
 public:
  void play(const Ts &...x) override { this->parent_->action_start_stream(); }
};

template<typename... Ts>
class UsbUvcStopStreamAction : public Action<Ts...>, public Parented<UsbUvcCamera> {
 public:
  void play(const Ts &...x) override { this->parent_->action_stop_stream(); }
};

}  // namespace usbuvc
}  // namespace esphome

#endif  // USE_ESP32
