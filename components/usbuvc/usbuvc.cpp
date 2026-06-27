#ifdef USE_ESP32

#include "usbuvc.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

#include <cstring>
#include <sstream>
#include <algorithm>
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

#ifdef CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
static bool uvc_enum_filter_callback(const usb_device_desc_t *dev_desc,
                                     uint8_t *bConfigurationValue) {
  ESP_LOGI("usbuvc_enum", "Device enumerated:");
  ESP_LOGI("usbuvc_enum", "  VID=0x%04X PID=0x%04X", dev_desc->idVendor, dev_desc->idProduct);
  ESP_LOGI("usbuvc_enum", "  bDeviceClass=0x%02X bDeviceSubClass=0x%02X",
           dev_desc->bDeviceClass, dev_desc->bDeviceSubClass);
  ESP_LOGI("usbuvc_enum", "  bcdUSB=0x%04X bMaxPacketSize0=%u",
           dev_desc->bcdUSB, dev_desc->bMaxPacketSize0);
  *bConfigurationValue = 1;  // use first configuration
  return true;  // enumerate this device
}
#endif

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
// UvcResolutionSelect
// ===========================================================================

#ifdef USE_SELECT
void UvcResolutionSelect::update_options(const std::vector<std::string> &options) {
  this->option_strings_ = options;  // copie pour garantir la durée de vie des c_str()

  // Reconstruit le FixedVector<const char *> à partir des strings stockées
  FixedVector<const char *> fv;
  fv.init(this->option_strings_.size());
  for (const auto &s : this->option_strings_)
    fv.push_back(s.c_str());

  this->traits.set_options(fv);

  // Sélectionne la première option par défaut si aucune n'est active
  if (!this->option_strings_.empty())
    this->publish_state(this->option_strings_[0]);
}

void UvcResolutionSelect::control(const std::string &value) {
  if (this->parent_ != nullptr)
    this->parent_->action_change_format(value);
  this->publish_state(value);
}
#endif  // USE_SELECT

#ifdef USE_NUMBER
// ===========================================================================
// UvcDownsamplingNumber
// ===========================================================================

void UvcDownsamplingNumber::setup() {
  // Publie la valeur initiale depuis le facteur configure dans usbuvc:
  if (this->parent_ != nullptr)
    this->publish_state(static_cast<float>(this->parent_->get_downsampling_factor()));
  else
    this->publish_state(1.0f);
}

void UvcDownsamplingNumber::control(float value) {
  uint8_t factor = static_cast<uint8_t>(value);
  if (factor < 1) factor = 1;
  if (this->parent_ != nullptr) {
    this->parent_->set_downsampling_factor(factor);
    ESP_LOGI("usbuvc", "downsampling_factor -> %u", (unsigned)factor);
  }
  this->publish_state(static_cast<float>(factor));
}
#endif  // USE_NUMBER

// ===========================================================================
// UsbUvcImage
// ===========================================================================

UsbUvcImage::UsbUvcImage(const uvc_host_frame_t *frame, uint8_t requesters,
                          uvc_host_stream_hdl_t stream_hdl)
    : data_(const_cast<uint8_t *>(frame->data)),
      len_(frame->data_len),
      requesters_(requesters),
      driver_frame_(frame),
      stream_hdl_(stream_hdl) {}

UsbUvcImage::UsbUvcImage(uint8_t *data, size_t len, uint8_t requesters)
    : data_(data), len_(len), requesters_(requesters),
      driver_frame_(nullptr), stream_hdl_(nullptr) {}

UsbUvcImage::~UsbUvcImage() {
  if (this->driver_frame_ != nullptr && this->stream_hdl_ != nullptr) {
    // Zero-copy: return buffer to UVC driver
    uvc_host_frame_return(this->stream_hdl_,
        const_cast<uvc_host_frame_t *>(this->driver_frame_));
  } else if (this->data_ != nullptr) {
    free(this->data_);  // NOLINT - malloc fallback
  }
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

  this->frame_queue_   = xQueueCreate(FRAME_QUEUE_DEPTH, sizeof(PendingFrame));
  this->stream_mutex_  = xSemaphoreCreateMutex();
  this->connect_event_ = xEventGroupCreate();

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
      "  Idle interval   : %lu ms\n"
      "  Frame buffers   : %u   URBs: %u x %lu B\n"
      "  Task USB lib    : prio=%u  stack=%u B\n"
      "  Task UVC conn   : prio=%u  stack=%u B\n"
      "  Downsampling    : 1 frame / %u",
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
      (unsigned)UVC_TASK_PRIO,       (unsigned)UVC_CONNECT_TASK_STACK,
      (unsigned)this->downsampling_factor_);

  if (this->is_failed())
    ESP_LOGE(TAG, "  Setup FAILED");
}

// ---------------------------------------------------------------------------
void UsbUvcCamera::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // --- Libere l'image courante si tous les readers sont termines -----------
  if (this->can_release_image_())
    this->current_image_.reset();

  // --- Idle heartbeat -------------------------------------------------------
  if (!this->has_requested_image_()) {
    if (this->idle_update_interval_ > 0 &&
        now - this->last_idle_request_ >= this->idle_update_interval_) {
      this->last_idle_request_ = now;
      this->single_requesters_ |= (1U << static_cast<uint8_t>(camera::IDLE));
    }
  }

  // --- Vide la queue des frames entrantes ----------------------------------
  // IMPORTANT : on depile TOUJOURS la queue pour rendre les buffers driver
  // le plus vite possible et eviter les UVC_HOST_FRAME_BUFFER_OVERFLOW.
  PendingFrame pf{nullptr};
  while (xQueueReceive(this->frame_queue_, &pf, 0) == pdTRUE) {
    if (pf.frame == nullptr || pf.frame->data_len == 0) {
      if (pf.frame) uvc_host_frame_return(this->uvc_stream_, const_cast<uvc_host_frame_t *>(pf.frame));
      continue;
    }

    // Rate limiting
    if (now - this->last_update_ < this->max_update_interval_) {
      // Trop tot : relachez le buffer et passez a la prochaine frame
      if (pf.frame != nullptr) uvc_host_frame_return(this->uvc_stream_, const_cast<uvc_host_frame_t *>(pf.frame));
      continue;
    }

    // Si pas de demande active : relache et continue
    if (!this->has_requested_image_()) {
      if (pf.frame != nullptr) uvc_host_frame_return(this->uvc_stream_, const_cast<uvc_host_frame_t *>(pf.frame));
      continue;
    }

    // Image courante encore en lecture : relache et sort
    // (on repassera au prochain cycle de loop)
    if (!this->can_release_image_()) {
      if (pf.frame != nullptr) uvc_host_frame_return(this->uvc_stream_, const_cast<uvc_host_frame_t *>(pf.frame));
      break;
    }

    // Dispatch
    this->dispatch_new_image_(pf);
    break;  // une seule image par cycle de loop
  }
}

// ===========================================================================
// Interface camera::Camera
// ===========================================================================

void UsbUvcCamera::request_image(camera::CameraRequester requester) {
  this->single_requesters_ |= (1U << static_cast<uint8_t>(requester));
}

void UsbUvcCamera::start_stream(camera::CameraRequester requester) {
  const uint8_t prev = this->stream_requesters_.fetch_or(
      static_cast<uint8_t>(1U << static_cast<uint8_t>(requester)));
  if (prev == 0) {
    // First requester: start hardware stream
    xSemaphoreTake(this->stream_mutex_, portMAX_DELAY);
    if (this->uvc_stream_ != nullptr)
      uvc_host_stream_start(this->uvc_stream_);
    xSemaphoreGive(this->stream_mutex_);
    this->fire_stream_start_triggers_();
    for (auto *l : this->listeners_)
      l->on_stream_start();
  }
}

void UsbUvcCamera::stop_stream(camera::CameraRequester requester) {
  const uint8_t prev = this->stream_requesters_.fetch_and(
      static_cast<uint8_t>(~(1U << static_cast<uint8_t>(requester))));
  const uint8_t mask = static_cast<uint8_t>(1U << static_cast<uint8_t>(requester));
  if ((prev & ~mask) == 0) {
    // Last requester: stop hardware stream
    xSemaphoreTake(this->stream_mutex_, portMAX_DELAY);
    if (this->uvc_stream_ != nullptr)
      uvc_host_stream_stop(this->uvc_stream_);
    xSemaphoreGive(this->stream_mutex_);
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
    return true;  // return immediately, nothing to do

  if (!this->has_requested_image_()) {
    return true;  // nobody wants a frame, return to driver
  }

  // Zero-copy: enqueue the driver frame pointer directly
  // We return FALSE to keep ownership of the buffer
  // loop() will call uvc_host_frame_return() after dispatch
  PendingFrame pf{frame};
  if (xQueueSendToBack(this->frame_queue_, &pf, 0) != pdPASS) {
    // Queue full: return buffer to driver immediately
    return true;
  }

  App.wake_loop_threadsafe();
  return false;  // we keep the buffer until loop() returns it
}

void UsbUvcCamera::on_stream_event_(const uvc_host_stream_event_data_t *event) {
  switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
      ESP_LOGE(TAG, "Erreur transfert USB: %d", event->transfer_error.error);
      break;

    case UVC_HOST_DEVICE_DISCONNECTED:
      ESP_LOGI(TAG, "Camera USB deconnectee");
      this->dev_connected_ = false;
      xSemaphoreTake(this->stream_mutex_, portMAX_DELAY);
      if (this->uvc_stream_ != nullptr) {
        // Drain frame_queue_ BEFORE close to avoid use-after-free:
        // pending frames point to driver buffers that close() will free.
        // We discard them without calling frame_return (driver handles cleanup).
        {
          PendingFrame stale;
          while (xQueueReceive(this->frame_queue_, &stale, 0) == pdTRUE) {}
        }
        // Also release current_image_ so its destructor does not call
        // uvc_host_frame_return() on an already-closed stream.
        this->current_image_.reset();
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
  // Downsampling : ne transmet qu'une frame sur downsampling_factor_
  // Les single_requesters (snapshot, idle) passent toujours sans downsampling.
  if (this->stream_requesters_ != 0 && this->downsampling_factor_ > 1) {
    this->downsampling_counter_++;
    if (this->downsampling_counter_ < this->downsampling_factor_) {
      if (pf.frame) uvc_host_frame_return(this->uvc_stream_, const_cast<uvc_host_frame_t *>(pf.frame));
      return;
    }
    this->downsampling_counter_ = 0;
  }

  const uint8_t req_mask = this->single_requesters_ | this->stream_requesters_;
  // Zero-copy: UsbUvcImage holds driver frame, returns it on destruction
  this->current_image_ = std::make_shared<UsbUvcImage>(
      pf.frame, req_mask, this->uvc_stream_);

  ESP_LOGV(TAG, "Nouveau frame : %u octets", (unsigned)pf.frame->data_len);

  for (auto *listener : this->listeners_)
    listener->on_camera_image(this->current_image_);

  UsbUvcCameraImageData img_data;
  img_data.data   = const_cast<uint8_t *>(pf.frame->data);
  img_data.length = pf.frame->data_len;
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


void UsbUvcCamera::publish_format_list_from_cache_() {
  if (true
#ifdef USE_TEXT_SENSOR
      && this->format_list_sensor_ == nullptr
#endif
#ifdef USE_SELECT
      && this->resolution_select_ == nullptr
#endif
  ) return;

  if (this->pending_formats_.empty()) {
#ifdef USE_TEXT_SENSOR
    if (this->format_list_sensor_) this->format_list_sensor_->publish_state("no_format");
#endif
    return;
  }

  // Collect valid MJPEG entries from cached format list
  // Intervals in 100ns units -> fps = 10 000 000 / interval
  struct FmtEntry { uint16_t w; uint16_t h; uint32_t fps; };
  std::vector<FmtEntry> entries;

  for (const auto &fi : this->pending_formats_) {
    if (fi.format != UVC_VS_FORMAT_MJPEG)
      continue;

    auto add = [&](uint32_t interval) {
      if (interval == 0) return;
      uint32_t fps = 10000000u / interval;
      if (fps < 1 || fps > 120) return;
      entries.push_back({(uint16_t)fi.h_res, (uint16_t)fi.v_res, fps});
    };

    if (fi.interval_type == 0) {
      add(fi.default_interval);
    } else {
      for (uint8_t k = 0; k < fi.interval_type; k++)
        add(fi.interval[k]);
    }
  }

  // Sort and deduplicate
  std::sort(entries.begin(), entries.end(), [](const FmtEntry &a, const FmtEntry &b) {
    uint32_t pa = (uint32_t)a.w * a.h;
    uint32_t pb = (uint32_t)b.w * b.h;
    if (pa != pb) return pa < pb;
    return a.fps < b.fps;
  });
  entries.erase(
    std::unique(entries.begin(), entries.end(), [](const FmtEntry &a, const FmtEntry &b) {
      return a.w == b.w && a.h == b.h && a.fps == b.fps;
    }),
    entries.end()
  );

  // Build option strings
  std::vector<std::string> opts;
  char buf[32];
  for (const auto &e : entries) {
    snprintf(buf, sizeof(buf), "%ux%u@%ufps", e.w, e.h, e.fps);
    opts.push_back(buf);
    ESP_LOGI(TAG, "  MJPEG: %s", buf);
  }

  if (opts.empty()) {
#ifdef USE_TEXT_SENSOR
    if (this->format_list_sensor_) this->format_list_sensor_->publish_state("no_mjpeg_format");
#endif
    return;
  }

#ifdef USE_TEXT_SENSOR
  if (this->format_list_sensor_ != nullptr) {
    std::string result;
    for (const auto &s : opts) {
      if (!result.empty()) result += "\n";
      result += s;
    }
    ESP_LOGI(TAG, "Formats MJPEG (%d): %s", (int)opts.size(), result.c_str());
    this->format_list_sensor_->publish_state(result);
  }
#endif

#ifdef USE_SELECT
  if (this->resolution_select_ != nullptr)
    this->resolution_select_->update_options(opts);
#endif
}

void UsbUvcCamera::on_driver_event_(const uvc_host_driver_event_data_t *event) {
  if (event->type == UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED) {
    const uint8_t dev_addr  = event->device_connected.dev_addr;
    const uint8_t stream_idx = event->device_connected.uvc_stream_index;
    const size_t  fmt_count  = event->device_connected.frame_info_num;
    ESP_LOGI(TAG, "Driver: camera connected addr=%u stream_idx=%u (%d formats)",
             dev_addr, stream_idx, (int)fmt_count);

    // Retrieve format list NOW, before stream_open.
    // After stream_open the config descriptor may no longer be accessible.
    this->pending_formats_.clear();
    if (fmt_count > 0) {
      auto *fl = new uvc_host_frame_info_t[fmt_count];
      if (fl != nullptr) {
        size_t actual = fmt_count;
        typedef uvc_host_frame_info_t fi_arr_t[];
        esp_err_t e = uvc_host_get_frame_list(
            dev_addr, stream_idx,
            reinterpret_cast<fi_arr_t *>(fl), &actual);
        if (e == ESP_OK) {
          for (size_t i = 0; i < actual; i++)
            this->pending_formats_.push_back(fl[i]);
          ESP_LOGI(TAG, "Format list cached: %d entries", (int)actual);
        } else {
          ESP_LOGW(TAG, "uvc_host_get_frame_list: %s", esp_err_to_name(e));
        }
        delete[] fl;
      }
    }

    this->pending_dev_addr_     = dev_addr;
    this->pending_stream_index_ = stream_idx;
    if (this->connect_event_ != nullptr)
      xEventGroupSetBits(this->connect_event_, EVT_DEVICE_CONNECTED);
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
    // Event-driven: wait for driver to confirm a compatible device is present
    // on_driver_event_() sets EVT_DEVICE_CONNECTED when UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED fires
    ESP_LOGI(TAG, "En attente d'un device UVC (VID=0x%04X PID=0x%04X)...",
             self->vid_, self->pid_);
    // Wait for driver event, with 10s timeout fallback in case the device
    // was already connected before this task started (missed event).
    EventBits_t bits = xEventGroupWaitBits(self->connect_event_,
                        EVT_DEVICE_CONNECTED,
                        pdTRUE,        // clear bit on exit
                        pdFALSE,
                        pdMS_TO_TICKS(60000));  // 60s fallback
    if ((bits & EVT_DEVICE_CONNECTED) == 0) {
      // Timeout after 60s: try with wildcard addr as last resort
      ESP_LOGW(TAG, "60s timeout: aucun event UVC recu -- tentative wildcard addr=0");
      self->pending_dev_addr_ = 0;
    }

    // Target the exact device addr reported by the driver
    cfg.usb.dev_addr = self->pending_dev_addr_;
    ESP_LOGI(TAG, "Device UVC detecte addr=%u -- ouverture %ux%u@%.0ffps",
             self->pending_dev_addr_,
             cfg.vs_format.h_res, cfg.vs_format.v_res, cfg.vs_format.fps);

    uvc_host_stream_hdl_t hdl = nullptr;
    esp_err_t err = uvc_host_stream_open(
        &cfg, pdMS_TO_TICKS(self->connect_timeout_ms_), &hdl);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "uvc_host_stream_open: %s -- re-essai",
               esp_err_to_name(err));
      // Re-arm immediately for a quick retry
      xEventGroupSetBits(self->connect_event_, EVT_DEVICE_CONNECTED);
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    xSemaphoreTake(self->stream_mutex_, portMAX_DELAY);
    self->uvc_stream_    = hdl;
    self->dev_connected_ = true;
    xSemaphoreGive(self->stream_mutex_);

    // Publish format list using the cache built in on_driver_event_
    self->publish_format_list_from_cache_();

    if (self->start_streaming_at_init_) {
      ESP_LOGI(TAG, "Camera UVC ouverte -- demarrage du stream (auto)");
      uvc_host_stream_start(hdl);
    } else {
      ESP_LOGI(TAG, "Camera UVC ouverte -- stream en attente (start_streaming_at_init=false)");
    }

    while (self->dev_connected_)
      vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Device deconnecte -- en attente du prochain event");
    // No vTaskDelay: the next loop iteration blocks on xEventGroupWaitBits
  }
}

}  // namespace usbuvc
}  // namespace esphome

#endif  // USE_ESP32
