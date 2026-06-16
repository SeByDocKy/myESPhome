#ifdef USE_ESP32

#include "usbuvc.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

#include <cstring>
#include <malloc.h>
#include "esp_heap_caps.h"

namespace esphome {
namespace usbuvc {

static const char *const TAG = "usbuvc";

// ===========================================================================
// Constantes dépendantes de la cible
//
// ESP32-S3 — USB Full-Speed (12 Mbps)
//   • RAM limitée (~512 KB DRAM, PSRAM externe via SPI)
//   • Micro-frame USB = 1 ms  → latence callback modérée
//   • Stacks modestes, priorités standard
//
// ESP32-P4 — USB High-Speed (480 Mbps)
//   • 32 MB PSRAM interne hautes performances
//   • Micro-frame USB = 125 µs → la tâche USB lib doit répondre très vite
//   • CPU RISC-V dual-core 400 MHz : on peut monter les priorités sans risque
//   • Stacks plus larges car le driver HS empile plus de contexte d'interruption
// ===========================================================================

#if CONFIG_IDF_TARGET_ESP32P4

// --- ESP32-P4 (USB High-Speed) -------------------------------------------
// Stacks : 6 KB pour la lib, 8 KB pour la tâche connect/UVC
// (le driver HS charge plus de contexte en isochronous haute bande passante)
static constexpr uint32_t    USB_LIB_TASK_STACK     = 6144;
static constexpr uint32_t    UVC_CONNECT_TASK_STACK  = 8192;
// Priorités hautes pour respecter les micro-frames à 125 µs
// (configMAX_PRIORITIES = 25 sur IDF ; on reste sous 20 pour laisser
//  de la marge au watchdog IDF qui tourne à priorité 22)
static constexpr UBaseType_t USB_LIB_TASK_PRIO       = 18;
static constexpr UBaseType_t UVC_TASK_PRIO            = 17;
// Affinité : Core 1 (Core 0 = main loop ESPHome + WiFi via esp32_hosted)
static constexpr BaseType_t  USB_LIB_CORE             = 1;
static constexpr BaseType_t  UVC_CONNECT_CORE         = 1;

#elif CONFIG_IDF_TARGET_ESP32S3

// --- ESP32-S3 (USB Full-Speed) --------------------------------------------
// Stacks : 4 KB suffit largement en FS
// Priorités : 15/14 — en-dessous des tâches WiFi IDF (~19) mais
//             au-dessus du main loop ESPHome (~5)
static constexpr uint32_t    USB_LIB_TASK_STACK     = 4096;
static constexpr uint32_t    UVC_CONNECT_TASK_STACK  = 4096;
static constexpr UBaseType_t USB_LIB_TASK_PRIO       = 15;
static constexpr UBaseType_t UVC_TASK_PRIO            = 14;
// tskNO_AFFINITY : le scheduler choisit le cœur disponible
static constexpr BaseType_t  USB_LIB_CORE             = tskNO_AFFINITY;
static constexpr BaseType_t  UVC_CONNECT_CORE         = tskNO_AFFINITY;

#else
// --- Autre cible ESP32 (S2, C3, …) : valeurs conservatives --------------
static constexpr uint32_t    USB_LIB_TASK_STACK     = 4096;
static constexpr uint32_t    UVC_CONNECT_TASK_STACK  = 4096;
static constexpr UBaseType_t USB_LIB_TASK_PRIO       = 15;
static constexpr UBaseType_t UVC_TASK_PRIO            = 14;
static constexpr BaseType_t  USB_LIB_CORE             = tskNO_AFFINITY;
static constexpr BaseType_t  UVC_CONNECT_CORE         = tskNO_AFFINITY;
#endif

// Profondeur de la queue inter-tâches (frames en attente de dispatch)
static constexpr uint8_t FRAME_QUEUE_DEPTH = 4;

// ===========================================================================
// Callbacks statiques UVC  (contexte : tâche driver UVC, pas le main loop)
// ===========================================================================

static void uvc_driver_event_callback(const uvc_host_driver_event_data_t *event, void *user_ctx) {
  auto *self = static_cast<UsbUvcCamera *>(user_ctx);
  if (self == nullptr || event == nullptr)
    return;
  self->on_driver_event_(event);
}

static bool uvc_frame_callback(const uvc_host_frame_t *frame, void *user_ctx) {
  auto *self = static_cast<UsbUvcCamera *>(user_ctx);
  if (self == nullptr || frame == nullptr)
    return true;
  return self->on_frame_(frame);
}

static void uvc_stream_callback(const uvc_host_stream_event_data_t *event, void *user_ctx) {
  auto *self = static_cast<UsbUvcCamera *>(user_ctx);
  if (self == nullptr || event == nullptr)
    return;
  self->on_stream_event_(event);
}

// ===========================================================================
// UsbUvcImage
// ===========================================================================

UsbUvcImage::UsbUvcImage(uint8_t *data, size_t len, uint8_t requesters)
    : data_(data), len_(len), requesters_(requesters) {}

UsbUvcImage::~UsbUvcImage() {
  free(this->data_);  // NOLINT(cppcoreguidelines-no-malloc)
  this->data_ = nullptr;
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

void UsbUvcImageReader::consume_data(size_t consumed) {
  this->offset_ += consumed;
}

void UsbUvcImageReader::return_image() {
  this->image_.reset();
}

// ===========================================================================
// Constructeurs des triggers
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
// UsbUvcCamera — cycle de vie ESPHome
// ===========================================================================

void UsbUvcCamera::setup() {
  // Enregistre cette instance comme la caméra globale.
  // L'API server appelle camera::Camera::instance() pour obtenir ce pointeur.
  camera::Camera::global_camera = this;

  this->last_update_       = millis();
  this->last_idle_request_ = millis();

  this->frame_queue_  = xQueueCreate(FRAME_QUEUE_DEPTH, sizeof(PendingFrame));
  this->stream_mutex_ = xSemaphoreCreateMutex();

  if (this->frame_queue_ == nullptr || this->stream_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Impossible d'allouer les primitives RTOS");
    this->mark_failed();
    return;
  }

  // --- Configuration du driver USB host ------------------------------------
  usb_host_config_t host_cfg = {};
  host_cfg.skip_phy_setup = false;
  host_cfg.intr_flags     = ESP_INTR_FLAG_LOWMED;


#if CONFIG_IDF_TARGET_ESP32P4
  // Sur P4 : la sélection du PHY HS vs FS se fait via sdkconfig
  // (CONFIG_USB_HOST_HW_INTERFACE_SELECT_HS) plutôt que par code.
  // Le champ 'port' ou 'peripheral_map' de usb_host_config_t n'existe
  // pas de manière stable avant IDF 5.5 — on laisse le driver utiliser
  // le PHY configuré par Kconfig (HS par défaut avec notre sdkconfig_options).
  ESP_LOGI(TAG, "ESP32-P4 : USB host (PHY sélectionné via sdkconfig)");
#else
  ESP_LOGI(TAG, "ESP32-S3 : PHY USB Full-Speed (GPIO19/20)");
#endif

  esp_err_t err = usb_host_install(&host_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_install: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // --- Tâche événements USB lib -------------------------------------------
  if (xTaskCreatePinnedToCore(
          UsbUvcCamera::usb_lib_task_, "usbuvc_lib",
          USB_LIB_TASK_STACK, nullptr,
          USB_LIB_TASK_PRIO, &this->usb_lib_task_handle_,
          USB_LIB_CORE) != pdTRUE) {
    ESP_LOGE(TAG, "Impossible de créer la tâche USB lib");
    this->mark_failed();
    return;
  }

  // --- Installation du driver UVC host ------------------------------------
  const uvc_host_driver_config_t uvc_drv_cfg = {
      .driver_task_stack_size = UVC_CONNECT_TASK_STACK,
      .driver_task_priority   = USB_LIB_TASK_PRIO + 1,
      .xCoreID                = USB_LIB_CORE,
      .create_background_task = true,
      .event_cb               = uvc_driver_event_callback,
      .user_ctx               = this,
  };
  err = uvc_host_install(&uvc_drv_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uvc_host_install: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // --- Tâche connexion / reconnexion UVC -----------------------------------
  if (xTaskCreatePinnedToCore(
          UsbUvcCamera::uvc_connect_task_, "usbuvc_conn",
          UVC_CONNECT_TASK_STACK, this,
          UVC_TASK_PRIO, &this->uvc_task_handle_,
          UVC_CONNECT_CORE) != pdTRUE) {
    ESP_LOGE(TAG, "Impossible de créer la tâche UVC connect");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "USB UVC camera setup OK "
#if CONFIG_IDF_TARGET_ESP32P4
      "(ESP32-P4, USB HS, prio=%d/%d, stack=%d/%d)",
#else
      "(ESP32-S3, USB FS, prio=%d/%d, stack=%d/%d)",
#endif
      (int)USB_LIB_TASK_PRIO, (int)UVC_TASK_PRIO,
      (int)USB_LIB_TASK_STACK, (int)UVC_CONNECT_TASK_STACK);
}

// ---------------------------------------------------------------------------
void UsbUvcCamera::dump_config() {
  ESP_LOGCONFIG(TAG,
      "USB UVC Camera:\n"
      "  Cible           : %s\n"
      "  Name            : %s\n"
      "  USB VID:PID     : 0x%04X:0x%04X  stream_idx=%u\n"
      "  Resolution      : %ux%u @ %u fps\n"
      "  Max interval    : %u ms\n"
      "  Idle interval   : %u ms\n"
      "  Frame buffers   : %u   URBs: %u x %u B\n"
      "  Task USB lib    : prio=%u  stack=%u B\n"
      "  Task UVC conn   : prio=%u  stack=%u B",
#if CONFIG_IDF_TARGET_ESP32P4
      "ESP32-P4 (USB HS)",
#elif CONFIG_IDF_TARGET_ESP32S3
      "ESP32-S3 (USB FS)",
#else
      "ESP32 (USB FS)",
#endif
      this->get_name().c_str(),
      this->vid_, this->pid_, this->uvc_stream_index_,
      this->width_, this->height_, this->fps_,
      this->max_update_interval_,
      this->idle_update_interval_,
      this->frame_buffer_count_,
      this->urb_count_, this->urb_size_,
      (unsigned)USB_LIB_TASK_PRIO,  (unsigned)USB_LIB_TASK_STACK,
      (unsigned)UVC_TASK_PRIO,       (unsigned)UVC_CONNECT_TASK_STACK);

  if (this->is_failed())
    ESP_LOGE(TAG, "  Setup FAILED");
}

// ---------------------------------------------------------------------------
void UsbUvcCamera::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  if (this->can_release_image_())
    this->current_image_.reset();

  if (!this->has_requested_image_()) {
    if (this->idle_update_interval_ > 0 &&
        now - this->last_idle_request_ >= this->idle_update_interval_) {
      this->last_idle_request_ = now;
      this->single_requesters_ |= (1U << static_cast<uint8_t>(camera::IDLE));
    } else {
      return;
    }
  }

  if (now - this->last_update_ < this->max_update_interval_)
    return;

  if (!this->can_release_image_())
    return;

  PendingFrame pf{};
  if (xQueueReceive(this->frame_queue_, &pf, 0) != pdTRUE)
    return;

  if (pf.data == nullptr || pf.len == 0) {
    free(pf.data);  // NOLINT
    return;
  }

  this->dispatch_new_image_(pf);
}

// ===========================================================================
// Interface camera::Camera
// ===========================================================================

void UsbUvcCamera::request_image(camera::CameraRequester requester) {
  this->single_requesters_ |= (1U << static_cast<uint8_t>(requester));
}

void UsbUvcCamera::start_stream(camera::CameraRequester requester) {
  const bool was_streaming = this->stream_requesters_ != 0;
  this->stream_requesters_ |= (1U << static_cast<uint8_t>(requester));
  // Ne fire les triggers qu'au premier start (transition 0 -> non-0)
  if (!was_streaming) {
    this->fire_stream_start_triggers_();
    for (auto *l : this->listeners_)
      l->on_stream_start();
  }
}

void UsbUvcCamera::stop_stream(camera::CameraRequester requester) {
  this->stream_requesters_ &= ~(1U << static_cast<uint8_t>(requester));
  // Ne fire les triggers que quand le dernier requester se retire (transition non-0 -> 0)
  if (this->stream_requesters_ == 0) {
    this->fire_stream_stop_triggers_();
    for (auto *l : this->listeners_)
      l->on_stream_stop();
  }
}

camera::CameraImageReader *UsbUvcCamera::create_image_reader() {
  return new UsbUvcImageReader();  // NOLINT(cppcoreguidelines-owning-memory)
}

// ===========================================================================
// Callback UVC — contexte tâche driver (pas le main loop)
// ===========================================================================

bool UsbUvcCamera::on_frame_(const uvc_host_frame_t *frame) {
  if (frame->data_len == 0)
    return true;

  if (!this->has_requested_image_())
    return true;

  uint8_t *copy = static_cast<uint8_t *>(
      heap_caps_malloc(frame->data_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));  // NOLINT
  if (copy == nullptr) {
    ESP_LOGW(TAG, "OOM — frame de %u octets ignoré", (unsigned)frame->data_len);
    return true;
  }
  memcpy(copy, frame->data, frame->data_len);

  PendingFrame pf{copy, frame->data_len};
  if (xQueueSendToBack(this->frame_queue_, &pf, 0) != pdPASS) {
    ESP_LOGV(TAG, "Queue pleine — frame ignoré");
    free(copy);  // NOLINT
  }

  App.wake_loop_threadsafe();
  return true;
}

void UsbUvcCamera::on_stream_event_(const uvc_host_stream_event_data_t *event) {
  switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
      ESP_LOGE(TAG, "Erreur transfert USB: %d", event->transfer_error.error);
      break;

    case UVC_HOST_DEVICE_DISCONNECTED:
      ESP_LOGI(TAG, "Caméra USB déconnectée");
      this->dev_connected_ = false;
      xSemaphoreTake(this->stream_mutex_, portMAX_DELAY);
      if (this->uvc_stream_ != nullptr) {
        uvc_host_stream_close(event->device_disconnected.stream_hdl);
        this->uvc_stream_ = nullptr;
      }
      xSemaphoreGive(this->stream_mutex_);
      this->fire_stream_stop_triggers_();
      for (auto *l : this->listeners_)
        l->on_stream_stop();
      break;

    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
      ESP_LOGW(TAG, "UVC frame buffer overflow");
      break;

    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
      ESP_LOGW(TAG, "UVC frame buffer underflow");
      break;

    default:
      ESP_LOGV(TAG, "Événement UVC: %d", (int)event->type);
      break;
  }
}

// ===========================================================================
// Helpers internes
// ===========================================================================

void UsbUvcCamera::dispatch_new_image_(PendingFrame &pf) {
  const uint8_t req_mask = this->single_requesters_ | this->stream_requesters_;
  this->current_image_ = std::make_shared<UsbUvcImage>(pf.data, pf.len, req_mask);

  ESP_LOGV(TAG, "Nouveau frame : %u octets", (unsigned)pf.len);

  for (auto *listener : this->listeners_)
    listener->on_camera_image(this->current_image_);

  UsbUvcCameraImageData img_data;
  img_data.data   = pf.data;
  img_data.length = pf.len;
  for (auto *t : this->image_triggers_)
    t->trigger(img_data);

  this->last_update_       = millis();
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
// Actions YAML
// ===========================================================================


void UsbUvcCamera::publish_format_list_(uint8_t dev_addr, uint8_t uvc_stream_index, size_t frame_info_num) {
  if (this->format_list_sensor_ == nullptr)
    return;

  if (frame_info_num == 0) {
    this->format_list_sensor_->publish_state("no_format");
    return;
  }

  // Alloue le tableau de frame_info
  auto *frame_list = new uvc_host_frame_info_t[frame_info_num];
  if (frame_list == nullptr) {
    ESP_LOGE(TAG, "publish_format_list_: OOM pour %d entrées", (int)frame_info_num);
    this->format_list_sensor_->publish_state("oom");
    return;
  }

  size_t actual = frame_info_num;
  esp_err_t err = uvc_host_get_frame_list(dev_addr, uvc_stream_index,
                                          (uvc_host_frame_info_t (*)[])frame_list,
                                          &actual);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uvc_host_get_frame_list: %s", esp_err_to_name(err));
    delete[] frame_list;
    this->format_list_sensor_->publish_state("error");
    return;
  }

  // Construit "WxH@Ffps|WxH@Ffps|..." (MJPEG uniquement)
  // Les intervalles sont en unités 100ns → fps = 10 000 000 / interval
  std::string result;
  char buf[48];
  for (size_t i = 0; i < actual; i++) {
    const uvc_host_frame_info_t &fi = frame_list[i];
    if (fi.format != UVC_VS_FORMAT_MJPEG)
      continue;

    if (fi.interval_type == 0) {
      // Continu : on publie default uniquement
      uint32_t fps = (fi.default_interval > 0) ? (10000000u / fi.default_interval) : 0;
      if (!result.empty()) result += "|";
      snprintf(buf, sizeof(buf), "%ux%u@%ufps", fi.h_res, fi.v_res, fps);
      result += buf;
      ESP_LOGI(TAG, "  MJPEG: %s (continu)", buf);
    } else {
      // Discret : un entry par intervalle
      for (uint8_t k = 0; k < fi.interval_type; k++) {
        uint32_t fps = (fi.interval[k] > 0) ? (10000000u / fi.interval[k]) : 0;
        if (!result.empty()) result += "|";
        snprintf(buf, sizeof(buf), "%ux%u@%ufps", fi.h_res, fi.v_res, fps);
        result += buf;
        ESP_LOGI(TAG, "  MJPEG: %s", buf);
      }
    }
  }

  delete[] frame_list;

  if (result.empty())
    result = "no_mjpeg_format";

  ESP_LOGI(TAG, "Formats MJPEG: %s", result.c_str());
  this->format_list_sensor_->publish_state(result);
}

void UsbUvcCamera::on_driver_event_(const uvc_host_driver_event_data_t *event) {
  if (event->type == UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED) {
    ESP_LOGI(TAG, "Driver: caméra connectée addr=%u stream_idx=%u (%d formats)",
             event->device_connected.dev_addr,
             event->device_connected.uvc_stream_index,
             (int)event->device_connected.frame_info_num);
    // Stocke dev_addr pour la reconnexion
    this->dev_addr_            = event->device_connected.dev_addr;
    this->connected_stream_index_ = event->device_connected.uvc_stream_index;
    // Publie la liste des formats dans le text_sensor
    this->publish_format_list_(
        event->device_connected.dev_addr,
        event->device_connected.uvc_stream_index,
        event->device_connected.frame_info_num);
  }
}

void UsbUvcCamera::action_change_format(const std::string &format) {
  // Parse "640x480@30fps"
  uint16_t w = 0, h = 0;
  unsigned int fps_uint = 0;
  if (sscanf(format.c_str(), "%hux%hu@%ufps", &w, &h, &fps_uint) != 3) {
    ESP_LOGE(TAG, "change_format: format invalide '%s' (attendu: WxH@Ffps)", format.c_str());
    return;
  }
  const uint8_t fps = static_cast<uint8_t>(fps_uint);

  xSemaphoreTake(this->stream_mutex_, portMAX_DELAY);
  if (this->uvc_stream_ == nullptr) {
    ESP_LOGW(TAG, "change_format: pas de stream ouvert");
    xSemaphoreGive(this->stream_mutex_);
    return;
  }

  uvc_host_stream_format_t new_fmt = {};
  new_fmt.h_res  = w;
  new_fmt.v_res  = h;
  new_fmt.fps    = static_cast<float>(fps);
  new_fmt.format = UVC_VS_FORMAT_MJPEG;

  ESP_LOGI(TAG, "change_format → %ux%u@%ufps", w, h, fps);
  esp_err_t err = uvc_host_stream_format_select(this->uvc_stream_, &new_fmt);
  if (err == ESP_OK) {
    this->width_  = w;
    this->height_ = h;
    this->fps_    = fps;
    ESP_LOGI(TAG, "change_format OK");
  } else {
    ESP_LOGE(TAG, "uvc_host_stream_format_select: %s", esp_err_to_name(err));
  }
  xSemaphoreGive(this->stream_mutex_);
}

void UsbUvcCamera::action_start_stream() {
  ESP_LOGI(TAG, "action: start_stream");
  xSemaphoreTake(this->stream_mutex_, portMAX_DELAY);
  if (this->uvc_stream_ != nullptr) {
    uvc_host_stream_start(this->uvc_stream_);
  } else {
    ESP_LOGW(TAG, "action_start_stream: pas de stream ouvert");
  }
  xSemaphoreGive(this->stream_mutex_);
}

void UsbUvcCamera::action_stop_stream() {
  ESP_LOGI(TAG, "action: stop_stream");
  xSemaphoreTake(this->stream_mutex_, portMAX_DELAY);
  if (this->uvc_stream_ != nullptr) {
    uvc_host_stream_stop(this->uvc_stream_);
  } else {
    ESP_LOGW(TAG, "action_stop_stream: pas de stream ouvert");
  }
  xSemaphoreGive(this->stream_mutex_);
}

// ===========================================================================
// Tâches RTOS
// ===========================================================================

void UsbUvcCamera::usb_lib_task_(void * /*pv*/) {
  while (true) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
      usb_host_device_free_all();
  }
}

void UsbUvcCamera::uvc_connect_task_(void *pv) {
  auto *self = static_cast<UsbUvcCamera *>(pv);

  uvc_host_stream_config_t cfg = {};
  cfg.event_cb  = uvc_stream_callback;
  cfg.frame_cb  = uvc_frame_callback;
  cfg.user_ctx  = self;

  cfg.usb.vid              = self->vid_;
  cfg.usb.pid              = self->pid_;
  cfg.usb.uvc_stream_index = self->uvc_stream_index_;

  cfg.vs_format.h_res  = self->width_;
  cfg.vs_format.v_res  = self->height_;
  cfg.vs_format.fps    = static_cast<float>(self->fps_);
  cfg.vs_format.format = UVC_VS_FORMAT_MJPEG;

  cfg.advanced.frame_size              = self->frame_size_max_;
  cfg.advanced.number_of_frame_buffers = self->frame_buffer_count_;
  cfg.advanced.number_of_urbs          = self->urb_count_;
  cfg.advanced.urb_size                = self->urb_size_;
  cfg.advanced.frame_heap_caps         = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

  while (true) {
    ESP_LOGI(TAG, "Tentative d'ouverture UVC 0x%04X:0x%04X %ux%u@%.0ffps …",
             cfg.usb.vid, cfg.usb.pid,
             cfg.vs_format.h_res, cfg.vs_format.v_res, cfg.vs_format.fps);

    uvc_host_stream_hdl_t hdl = nullptr;
    esp_err_t err = uvc_host_stream_open(&cfg, pdMS_TO_TICKS(5000), &hdl);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "uvc_host_stream_open: %s — nouvelle tentative dans 5 s",
               esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    xSemaphoreTake(self->stream_mutex_, portMAX_DELAY);
    self->uvc_stream_    = hdl;
    self->dev_connected_ = true;
    xSemaphoreGive(self->stream_mutex_);

    ESP_LOGI(TAG, "Caméra UVC ouverte — démarrage du stream");
    uvc_host_stream_start(hdl);


    while (self->dev_connected_)
      vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Appareil retiré — pause 3 s avant reconnexion");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

}  // namespace usbuvc
}  // namespace esphome

#endif  // USE_ESP32
