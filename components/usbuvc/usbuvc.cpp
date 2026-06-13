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

// Tailles de stack / priorités — ajustées pour ESP32-S3
static constexpr uint32_t    USB_LIB_TASK_STACK    = 4096;
static constexpr uint32_t    UVC_CONNECT_TASK_STACK = 4096;
static constexpr UBaseType_t USB_LIB_TASK_PRIO      = 15;
static constexpr UBaseType_t UVC_TASK_PRIO           = 14;

// Profondeur de la queue inter-tâches (frames en attente de dispatch)
static constexpr uint8_t FRAME_QUEUE_DEPTH = 4;

// ===========================================================================
// Callbacks statiques UVC  (contexte : tâche driver UVC, pas le main loop)
// ===========================================================================

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
  // -----------------------------------------------------------------------
  // CRITIQUE : enregistre cette instance comme la caméra globale.
  // L'API server appelle camera::Camera::instance() pour obtenir ce pointeur,
  // puis appelle request_image() / start_stream() / create_image_reader().
  // Sans cette ligne, HA ne voit pas de caméra.
  // -----------------------------------------------------------------------
  camera::Camera::global_camera = this;

  this->last_update_       = millis();
  this->last_idle_request_ = millis();

  // File RTOS : la tâche UVC y dépose des PendingFrame,
  // le main loop ESPHome les consomme dans loop().
  this->frame_queue_  = xQueueCreate(FRAME_QUEUE_DEPTH, sizeof(PendingFrame));
  this->stream_mutex_ = xSemaphoreCreateMutex();

  if (this->frame_queue_ == nullptr || this->stream_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Impossible d'allouer les primitives RTOS");
    this->mark_failed();
    return;
  }

  // --- Installation du driver USB host (une seule fois par firmware) ------
  const usb_host_config_t host_cfg = {
      .skip_phy_setup = false,
      .intr_flags     = ESP_INTR_FLAG_LOWMED,
  };
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
          tskNO_AFFINITY) != pdTRUE) {
    ESP_LOGE(TAG, "Impossible de créer la tâche USB lib");
    this->mark_failed();
    return;
  }

  // --- Installation du driver UVC host ------------------------------------
  const uvc_host_driver_config_t uvc_drv_cfg = {
      .driver_task_stack_size = 4096,
      .driver_task_priority   = USB_LIB_TASK_PRIO + 1,
      .xCoreID                = tskNO_AFFINITY,
      .create_background_task = true,
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
          tskNO_AFFINITY) != pdTRUE) {
    ESP_LOGE(TAG, "Impossible de créer la tâche UVC connect");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "USB UVC camera setup OK");
}

// ---------------------------------------------------------------------------
void UsbUvcCamera::dump_config() {
  ESP_LOGCONFIG(TAG,
      "USB UVC Camera:\n"
      "  Name            : %s\n"
      "  USB VID:PID     : 0x%04X:0x%04X  stream_idx=%u\n"
      "  Resolution      : %ux%u @ %u fps\n"
      "  Max interval    : %u ms\n"
      "  Idle interval   : %u ms\n"
      "  Frame buffers   : %u   URBs: %u x %u B",
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
// loop() — exécuté dans le contexte du main loop ESPHome (core 0 ou 1)
// ---------------------------------------------------------------------------
void UsbUvcCamera::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // -- Libère l'image courante si tous les readers ont terminé -------------
  if (this->can_release_image_())
    this->current_image_.reset();

  // -- Heartbeat idle : maintient la miniature HA à jour ------------------
  if (!this->has_requested_image_()) {
    if (this->idle_update_interval_ > 0 &&
        now - this->last_idle_request_ >= this->idle_update_interval_) {
      this->last_idle_request_ = now;
      // Demande un snapshot pour le mode idle (miniature HA)
      this->single_requesters_ |= (1U << static_cast<uint8_t>(camera::IDLE));
    } else {
      return;
    }
  }

  // -- Rate limiting -------------------------------------------------------
  if (now - this->last_update_ < this->max_update_interval_)
    return;

  // -- Si une image est encore en cours de lecture, on attend --------------
  if (!this->can_release_image_())
    return;

  // -- Tente de dépiler un frame depuis la tâche UVC ----------------------
  PendingFrame pf{};
  if (xQueueReceive(this->frame_queue_, &pf, 0) != pdTRUE)
    return;  // rien de prêt

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
  // Positionne le bit correspondant : le prochain frame disponible sera livré.
  this->single_requesters_ |= (1U << static_cast<uint8_t>(requester));
}

void UsbUvcCamera::start_stream(camera::CameraRequester requester) {
  this->stream_requesters_ |= (1U << static_cast<uint8_t>(requester));
  this->fire_stream_start_triggers_();
  // Notifie les listeners (API server → on_stream_start côté HA)
  for (auto *l : this->listeners_)
    l->on_stream_start();
}

void UsbUvcCamera::stop_stream(camera::CameraRequester requester) {
  this->stream_requesters_ &= ~(1U << static_cast<uint8_t>(requester));
  this->fire_stream_stop_triggers_();
  for (auto *l : this->listeners_)
    l->on_stream_stop();
}

camera::CameraImageReader *UsbUvcCamera::create_image_reader() {
  // L'API server appelle ceci pour chaque connexion client HA.
  // Chaque reader retient un shared_ptr sur current_image_ ; tant qu'un
  // reader existe, l'image n'est pas libérée.
  return new UsbUvcImageReader();  // NOLINT(cppcoreguidelines-owning-memory)
}

// ===========================================================================
// Callback UVC — contexte tâche driver (pas le main loop)
// ===========================================================================

bool UsbUvcCamera::on_frame_(const uvc_host_frame_t *frame) {
  // Retourner 'true' = on rend le buffer au driver immédiatement.
  // On copie le payload AVANT de retourner pour ne pas bloquer le driver.

  if (frame->data_len == 0)
    return true;

  // Ne copie que si quelqu'un attend un frame
  if (!this->has_requested_image_())
    return true;

  uint8_t *copy = static_cast<uint8_t *>(malloc(frame->data_len));  // NOLINT
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

  // Réveille le main loop pour qu'il traite le frame sans attendre
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
      // Notifie HA que le flux s'est arrêté
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
      ESP_LOGV(TAG, "Événement UVC inattendu: %d", (int)event->type);
      break;
  }
}

// ===========================================================================
// Helpers internes
// ===========================================================================

void UsbUvcCamera::dispatch_new_image_(PendingFrame &pf) {
  // Construit un UsbUvcImage qui prend ownership du malloc (libéré dans ~UsbUvcImage)
  const uint8_t req_mask = this->single_requesters_ | this->stream_requesters_;
  this->current_image_ = std::make_shared<UsbUvcImage>(pf.data, pf.len, req_mask);

  ESP_LOGV(TAG, "Nouveau frame : %u octets", (unsigned)pf.len);

  // -----------------------------------------------------------------------
  // Notifie tous les listeners — l'API server est ici.
  // Il reçoit on_camera_image(), récupère le shared_ptr, et appelle
  // create_image_reader() → set_image() pour streamer vers HA via WebSocket.
  // -----------------------------------------------------------------------
  for (auto *listener : this->listeners_)
    listener->on_camera_image(this->current_image_);

  // Fire on_image trigger (automations YAML)
  UsbUvcCameraImageData img_data;
  img_data.data   = pf.data;
  img_data.length = pf.len;
  for (auto *t : this->image_triggers_)
    t->trigger(img_data);

  this->last_update_       = millis();
  // Acquitte les demandes one-shot ; les flux continus (stream_requesters_) restent actifs
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
// Tâches RTOS
// ===========================================================================

// Tâche événements bas-niveau USB host (doit tourner en permanence)
void UsbUvcCamera::usb_lib_task_(void * /*pv*/) {
  while (true) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
      usb_host_device_free_all();
  }
}

// Tâche connexion UVC : ouvre le stream et reconnecte automatiquement
void UsbUvcCamera::uvc_connect_task_(void *pv) {
  auto *self = static_cast<UsbUvcCamera *>(pv);

  uvc_host_stream_config_t cfg = {};
  // Passe 'self' en user_ctx pour éviter le singleton global
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

    // Attend la déconnexion (on_stream_event_ met dev_connected_ à false)
    while (self->dev_connected_)
      vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Appareil retiré — pause 3 s avant reconnexion");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

}  // namespace usbuvc
}  // namespace esphome

#endif  // USE_ESP32
