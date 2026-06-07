# usbuvc — ESPHome native USB UVC camera component

Streams MJPEG video from a USB UVC camera to Home Assistant through the
native ESPHome camera API, exactly like the built-in `esp32_camera` component
does for parallel-bus OV2640/OV5640 sensors.

## Architecture

```
USB camera (MJPEG)
        │  USB FS
        ▼
  ESP32-S3 USB OTG port
        │
  ┌─────┴────────────────────────────────┐
  │  usb_host + uvc_host (ESP-IDF 5.x)  │
  │                                      │
  │  uvc_connect_task  ──────────────►  │  opens / reopens stream
  │  UVC frame callback  ──copy──►       │
  │  frame_queue (FreeRTOS)              │
  └─────┬────────────────────────────────┘
        │  loop() (main ESPHome task)
        ▼
  UsbUvcImage  (shared_ptr, owns malloc'd copy)
        │
        ▼
  camera::Camera listeners
  (ESPHome API server → Home Assistant)
```

## Requirements

| Item | Detail |
|------|--------|
| SoC  | ESP32-S2 or ESP32-S3 (native USB OTG) |
| Framework | `esp-idf` ≥ 5.0 (Arduino is **not** supported) |
| PSRAM | Required – frame buffers live in SPIRAM |
| Camera | USB Full-Speed, UVC class, MJPEG format |
| Power | External 5 V for the camera – DevKitC USB port cannot supply enough current |

## File structure

```
components/
  usbuvc/
    __init__.py        ← ESPHome Python schema + codegen
    usbuvc.h           ← C++ class declaration
    usbuvc.cpp         ← C++ implementation
    idf_component.yml  ← IDF Component Manager dependency declaration
example.yaml           ← Example ESPHome YAML
README.md              ← This file
```

## Usage

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [usbuvc]

usbuvc:
  name: "USB Camera"
  resolution: 640x480
  fps: 15
  max_framerate: 15 fps
  idle_framerate: 0.1 fps
```

## Configuration reference

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `name` | string | **required** | Entity name in Home Assistant |
| `vid` | hex int | `0` | USB Vendor ID (0 = any) |
| `pid` | hex int | `0` | USB Product ID (0 = any) |
| `uvc_stream_index` | int 0-7 | `0` | Which UVC function to use (0 = first) |
| `resolution` | enum | `640x480` | `160x120` / `320x240` / `640x480` / `800x600` / `1024x768` / `1280x720` / `1280x960` / `1920x1080` |
| `fps` | int 1-60 | `15` | Requested frame rate |
| `max_framerate` | framerate | `15 fps` | Maximum push rate to HA |
| `idle_framerate` | framerate | `0.1 fps` | Push rate when no client is streaming |
| `frame_buffer_count` | int 1-8 | `3` | UVC frame buffers (in PSRAM) |
| `urb_count` | int 1-8 | `3` | USB transfer buffers |
| `urb_size` | int | `10240` | Size of each URB in bytes |
| `frame_size` | int | `0` | Max frame bytes (0 = from negotiation) |
| `on_stream_start` | automation | — | Fires when the device connects and streaming begins |
| `on_stream_stop` | automation | — | Fires when the device disconnects |
| `on_image` | automation | — | Fires for every captured frame (`image.data`, `image.length`) |

## Known limitations

* Only **MJPEG** format is supported (matches what the ESPHome API layer
  expects and what most cheap FS cameras advertise).
* USB **Full-Speed** (12 Mbps) only. High-Speed cameras will be opened at FS
  if they support it, otherwise they won't connect.
* A single UVC stream per firmware instance.
* The ESP32-S3 DevKitC-1 cannot supply the ~500 mA a typical USB camera
  needs – use an external USB hub with its own power supply or wire 5 V
  directly to the camera VBUS.

## How frames reach Home Assistant

The component inherits from `camera::Camera` (the same abstract base used by
`esp32_camera`). The ESPHome native API server registers itself as a listener
and forwards MJPEG frames over the encrypted WebSocket to Home Assistant,
which then exposes a standard `camera` entity.

## Reconnection

The `uvc_connect_task` automatically retries every 5 seconds if no device is
found, and reconnects after a disconnect (e.g. cable unplug).

## Credits

* Espressif `esp-idf` UVC host example:
  `examples/peripherals/usb/host/uvc/main/main.c`
* ESPHome `esp32_camera` component (streaming architecture)
* `esphome_webcam` by anton-malakhov (prior art, uses `usb_stream` from
  esp-iot-solution instead of the newer `uvc_host`)
