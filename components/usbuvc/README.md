# usbuvc — ESPHome USB UVC Camera Component

Native ESPHome external component that streams MJPEG video from a USB UVC camera to Home Assistant via the native ESPHome API, exactly like the built-in `esp32_camera` component does for parallel-bus sensors — but using USB instead.

**Tested:** SunplusIT `VID=0x1BCF PID=0x0951` up to **2560×1440 @ 30 fps** on ESP32-P4.

---

## Features

- **Zero-copy pipeline** — frames go directly from the UVC driver buffer to the HA API with no intermediate `memcpy`, enabling high-resolution streaming without overflow
- **Auto format discovery** — lists all MJPEG formats advertised by the camera at connection time
- **Live resolution switching** — change resolution/fps on the fly from HA without restarting the stream
- **Downsampling slider** — reduce the frame rate sent to HA independently of the capture rate
- **Automatic reconnection** — reconnects transparently when the camera is unplugged and replugged
- **ESP32-S3 and ESP32-P4** — target-specific RTOS tuning (stack sizes, priorities, USB HS/FS)
- **Three optional sub-platforms:** `text_sensor`, `select`, `number`

---

## Hardware Requirements

| Item | Detail |
|---|---|
| SoC | ESP32-S3 (USB FS, 12 Mbps) or ESP32-P4 (USB HS, 480 Mbps) |
| Framework | `esp-idf` >= 5.0 (Arduino **not** supported) |
| Camera | USB UVC class, MJPEG format |
| Power | External 5 V supply for the camera (DevKitC cannot source enough current) |

### Wiring — ESP32-S3 DevKitC-1

| Signal | GPIO | Note |
|---|---|---|
| D− | GPIO 19 | Dedicated USB OTG, not remappable |
| D+ | GPIO 20 | Dedicated USB OTG, not remappable |
| VBUS | VIN | External 5 V >= 500 mA |
| GND | GND | Common ground — mandatory |

Use a USB-A female breakout board on a breadboard. Keep D+/D− wires under 10 cm.

### Wiring — ESP32-P4 Function EV Board v1.4

Plug the camera directly into the **USB Type-A** port on the board. D±/VBUS are wired internally via a TPS2051C power switch. No additional connections needed.

---

## File Structure

```
components/usbuvc/
├── __init__.py          ← ESPHome schema + codegen
├── usbuvc.h             ← C++ declarations
├── usbuvc.cpp           ← C++ implementation
├── idf_component.yml    ← IDF Component Manager dependency (espressif/usb_host_uvc)
├── text_sensor/
│   └── __init__.py      ← optional: format list text sensor platform
├── select/
│   └── __init__.py      ← optional: resolution select platform
└── number/
    └── __init__.py      ← optional: downsampling slider platform
```

---

## Minimal Configuration

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [usbuvc]

usbuvc:
  id: my_webcam
  name: "USB Camera"
```

---

## Full Configuration Reference

```yaml
usbuvc:
  id: my_webcam
  name: "USB Camera"

  # USB device filter — 0 = match any device
  vid: 0x1BCF
  pid: 0x0951
  uvc_stream_index: 0        # 0 = first UVC function on the device

  # Video format
  resolution: 640X480        # see supported resolutions below
  fps: 30

  # Frame delivery to HA
  max_framerate: 15 fps      # max push rate to HA
  idle_framerate: 0.1 fps    # push rate when no client is streaming (thumbnail)

  # USB transfer tuning
  frame_buffer_count: 3      # number of internal UVC driver frame buffers
  urb_count: 3               # number of USB transfer buffers
  urb_size: 10240            # bytes per URB (increase for HS cameras, e.g. 16384)
  frame_size_max: 0          # max MJPEG frame size in bytes (0 = auto-detect on first frame)

  # Control transfer buffer size
  # Must be >= wTotalLength of the USB configuration descriptor
  # Cameras with many format descriptors require 2048 (default)
  transfer_max_size: 2048

  # Stream control
  start_streaming_at_init: true   # start stream automatically on boot
  downsampling_factor: 1          # send 1 frame out of N to HA (1 = all frames)

  # Automations
  on_stream_start:
    - logger.log: "Stream started"
  on_stream_stop:
    - logger.log: "Stream stopped"
  on_image:               # fires for every frame dispatched to HA
    then:
      - logger.log:
          format: "Frame %d bytes"
          args: [image.length]
```

### Supported Resolutions

`160X120` · `320X240` · `640X480` · `800X600` · `1024X768` · `1280X720` · `1280X960` · `1920X1080`

The camera must advertise the requested resolution + fps combination as a MJPEG format.
Use the `text_sensor` platform to discover what your camera actually supports.

---

## sdkconfig — ESP32-P4

```yaml
esp32:
  board: esp32-p4-function-ev-board
  variant: esp32p4
  framework:
    type: esp-idf
    version: recommended
    sdkconfig_options:
      CONFIG_ESP_CONSOLE_USB_CDC: "n"        # disable USB CDC console
      CONFIG_ESP_CONSOLE_UART_DEFAULT: "y"   # use UART0 for logs
      CONFIG_USB_HOST_HW_INTERFACE_SELECT_HS: "y"  # use HS PHY (GPIO49/50)
```

> `transfer_max_size` in the YAML automatically injects `CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE` into sdkconfig. No need to set both.

---

## sdkconfig — ESP32-S3

```yaml
esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  framework:
    type: esp-idf
    version: recommended
    sdkconfig_options:
      CONFIG_ESP_CONSOLE_USB_CDC: "n"
      CONFIG_ESP_CONSOLE_UART_DEFAULT: "y"

psram:
  mode: ocpi
  speed: 80MHz
```

---

## Optional Sub-Platforms

### `text_sensor` — Format List

Publishes all MJPEG formats supported by the connected camera, sorted by resolution then fps, separated by newlines. Updated automatically at each connection.

```yaml
text_sensor:
  - platform: usbuvc
    name: "Camera formats"
    usbuvc_id: my_webcam
```

Example published value:
```
640x480@30fps
800x600@30fps
1280x720@30fps
1280x960@30fps
1920x1080@30fps
2560x1440@30fps
```

---

### `select` — Live Resolution Switching

A select entity populated automatically with all available MJPEG formats at connection time. Selecting an option calls `uvc_host_stream_format_select()` without closing the stream.

```yaml
select:
  - platform: usbuvc
    name: "Camera resolution"
    usbuvc_id: my_webcam
```

---

### `number` — Downsampling Slider

Controls `downsampling_factor` at runtime. With `fps: 30` and factor `3`, HA receives 10 fps while the camera still captures at 30 fps. Single-frame snapshot requests always pass through regardless of the factor.

```yaml
number:
  - platform: usbuvc
    name: "Downsampling"
    usbuvc_id: my_webcam
    min_value: 1
    max_value: 5
    step: 1
```

---

## Actions

### `usbuvc.start_stream`

Starts the UVC stream on the already-opened device.

```yaml
- usbuvc.start_stream: my_webcam
```

### `usbuvc.stop_stream`

Stops the UVC stream without closing the USB device.

```yaml
- usbuvc.stop_stream: my_webcam
```

### `usbuvc.change_format`

Changes the stream resolution and fps on the fly. Format string: `WxH@Ffps`.

```yaml
- usbuvc.change_format:
    id: my_webcam
    format: "1280x720@30fps"

# With lambda (e.g. driven by a select on_value):
- usbuvc.change_format:
    id: my_webcam
    format: !lambda return x;
```

---

## Complete Example — ESP32-P4

```yaml
esphome:
  name: usb-camera-p4
  friendly_name: "USB Camera P4"

esp32:
  board: esp32-p4-function-ev-board
  variant: esp32p4
  framework:
    type: esp-idf
    version: recommended
    sdkconfig_options:
      CONFIG_ESP_CONSOLE_USB_CDC: "n"
      CONFIG_ESP_CONSOLE_UART_DEFAULT: "y"
      CONFIG_USB_HOST_HW_INTERFACE_SELECT_HS: "y"

psram:
  speed: 200MHz

logger:
  hardware_uart: UART0

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret api_key

ota:
  - platform: esphome
    password: !secret ota_password

external_components:
  - source:
      type: local
      path: components
    components: [usbuvc]

usbuvc:
  id: my_webcam
  name: "USB Camera"
  vid: 0x1BCF
  pid: 0x0951
  resolution: 640X480
  fps: 30
  frame_buffer_count: 6
  urb_count: 4
  urb_size: 16384
  transfer_max_size: 2048
  start_streaming_at_init: false
  downsampling_factor: 1
  on_stream_start:
    - logger.log: "Stream started"
  on_stream_stop:
    - logger.log: "Stream stopped"

text_sensor:
  - platform: usbuvc
    name: "Camera formats"
    usbuvc_id: my_webcam

select:
  - platform: usbuvc
    name: "Camera resolution"
    usbuvc_id: my_webcam

number:
  - platform: usbuvc
    name: "Downsampling"
    usbuvc_id: my_webcam
    min_value: 1
    max_value: 5

button:
  - platform: template
    name: "Start stream"
    on_press:
      - usbuvc.start_stream: my_webcam

  - platform: template
    name: "Stop stream"
    on_press:
      - usbuvc.stop_stream: my_webcam
```

---

## Architecture

```
USB Camera (MJPEG)
    |  USB HS (ESP32-P4) or FS (ESP32-S3)
    v
usb_host + uvc_host driver  <-- frame_buffer_count internal frame buffers
    |
    |  on_frame_()  [UVC driver task, ~1 us]
    |  +-- return false  <- keep buffer ownership, zero memcpy
    |  +-- enqueue frame* pointer (8 bytes)
    |
    |  loop()  [ESPHome main loop]
    |  +-- dequeue frame*
    |  +-- downsampling check
    |  +-- UsbUvcImage{frame*}  <- zero-copy wrapper
    |  +-- notify API server listeners
    |  +-- ~UsbUvcImage() -> uvc_host_frame_return()  <- return to driver
    |
    v
ESPHome API server -> WebSocket -> Home Assistant camera entity
```

### Zero-Copy Design

`on_frame_()` returns `false` to retain ownership of the driver frame buffer.
`UsbUvcImage` wraps the driver pointer with no data copy.
When the last `shared_ptr` to the image is released (after HA has finished reading),
`~UsbUvcImage()` calls `uvc_host_frame_return()` to return the buffer to the driver.

This eliminates the bottleneck that caused `UVC_HOST_FRAME_BUFFER_OVERFLOW` at high resolutions.

---

## Tuning Guide

| Symptom | Cause | Fix |
|---|---|---|
| `ESP_ERR_NOT_FOUND` at boot | Camera not enumerated | Check VBUS power and D+/D-/GND wiring |
| `Configuration descriptor larger than control transfer max length` | `transfer_max_size` too small | Set `transfer_max_size: 2048` |
| `Could not find frame format WxH@Ffps` | fps not supported at that resolution | Check `text_sensor` format list, adjust `fps` |
| `UVC frame buffer overflow` | Too few driver buffers or URBs too small | Increase `frame_buffer_count` to 6-8, `urb_size` to 16384 for HS |
| `Pool epuise` | Old version with malloc-based pool | Update to v2 zero-copy |
| Low fps received in HA | `max_framerate` cap or `downsampling_factor` | Adjust `max_framerate` and `downsampling_factor` |
| First compile fails with missing header | IDF Component Manager first-run delay | Run `esphome compile` a second time |

---

## Limitations

- **One camera per firmware** — `camera::Camera` in ESPHome is a singleton; two `usbuvc:` blocks will not work
- **MJPEG only** — YUV/H264/H265 formats are not forwarded to HA (the ESPHome camera API expects MJPEG)
- **ESP32-P4 requires companion WiFi chip** — the P4 has no integrated WiFi; use `esp32_hosted:` with an ESP32-C6

---

## Credits

- Espressif `esp-idf` UVC host driver and `usb_host_uvc` managed component
- ESPHome `esp32_camera` component (streaming architecture reference)
- Developed with [SeByDocKy/myESPhome](https://github.com/SeByDocKy/myESPhome)
