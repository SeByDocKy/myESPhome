"""ESPHome native component for USB UVC cameras (ESP32-S2/S3).

Streams MJPEG video to Home Assistant via the native camera API,
mirroring the behaviour of esp32_camera but sourcing frames from
a USB Video Class device instead of a parallel-bus sensor.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)
from esphome.core.entity_helpers import setup_entity

DEPENDENCIES = ["esp32", "psram"]
AUTO_LOAD = ["camera"]

# ------------------------------------------------------------------ namespace
usbuvc_ns = cg.esphome_ns.namespace("usbuvc")

camera_ns = cg.esphome_ns.namespace("camera")
CameraBase = camera_ns.class_("Camera", cg.EntityBase, cg.Component)

UsbUvcCamera = usbuvc_ns.class_("UsbUvcCamera", CameraBase)

# Trigger types
UsbUvcStreamStartTrigger = usbuvc_ns.class_(
    "UsbUvcStreamStartTrigger", automation.Trigger.template()
)
UsbUvcStreamStopTrigger = usbuvc_ns.class_(
    "UsbUvcStreamStopTrigger", automation.Trigger.template()
)
UsbUvcCameraImageData = usbuvc_ns.struct("UsbUvcCameraImageData")
UsbUvcImageTrigger = usbuvc_ns.class_(
    "UsbUvcImageTrigger", automation.Trigger.template(UsbUvcCameraImageData)
)

# Action types
UsbUvcStartStreamAction = usbuvc_ns.class_(
    "UsbUvcStartStreamAction", automation.Action
)
UsbUvcStopStreamAction = usbuvc_ns.class_(
    "UsbUvcStopStreamAction", automation.Action
)
UsbUvcChangeFormatAction = usbuvc_ns.class_(
    "UsbUvcChangeFormatAction", automation.Action
)

# ---------------------------------------------------------------- config keys
CONF_VID                    = "vid"
CONF_PID                    = "pid"
CONF_UVC_STREAM_INDEX       = "uvc_stream_index"
CONF_FPS                    = "fps"
CONF_MAX_FRAMERATE          = "max_framerate"
CONF_IDLE_FRAMERATE         = "idle_framerate"
CONF_FRAME_BUFFER_COUNT     = "frame_buffer_count"
CONF_URB_COUNT              = "urb_count"
CONF_URB_SIZE               = "urb_size"
CONF_FRAME_SIZE_MAX         = "frame_size_max"
CONF_TRANSFER_MAX_SIZE      = "transfer_max_size"
CONF_ON_STREAM_START        = "on_stream_start"
CONF_ON_STREAM_STOP         = "on_stream_stop"
CONF_ON_IMAGE               = "on_image"

# ---------------------------------------------------------------- resolution
RESOLUTIONS = {
    "160X120":   (160,  120),
    "320X240":   (320,  240),
    "640X480":   (640,  480),
    "800X600":   (800,  600),
    "1024X768":  (1024, 768),
    "1280X720":  (1280, 720),
    "1280X960":  (1280, 960),
    "1920X1080": (1920, 1080),
}

# ---------------------------------------------------------------- main schema
CONFIG_SCHEMA = cv.All(
    cv.ENTITY_BASE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(UsbUvcCamera),
            cv.Optional(CONF_VID, default=0): cv.hex_int,
            cv.Optional(CONF_PID, default=0): cv.hex_int,
            cv.Optional(CONF_UVC_STREAM_INDEX, default=0): cv.int_range(min=0, max=7),
            cv.Optional("resolution", default="640X480"): cv.enum(
                {k: k for k in RESOLUTIONS}, upper=True
            ),
            cv.Optional(CONF_FPS, default=15): cv.int_range(min=1, max=60),
            cv.Optional(CONF_MAX_FRAMERATE, default="15 fps"): cv.All(
                cv.framerate, cv.Range(min=0, min_included=False, max=60)
            ),
            cv.Optional(CONF_IDLE_FRAMERATE, default="0.1 fps"): cv.All(
                cv.framerate, cv.Range(min=0, max=1)
            ),
            cv.Optional(CONF_FRAME_BUFFER_COUNT, default=3): cv.int_range(min=1, max=8),
            cv.Optional(CONF_URB_COUNT, default=3): cv.int_range(min=1, max=8),
            cv.Optional(CONF_URB_SIZE, default=10240): cv.int_range(min=512, max=65536),
            cv.Optional(CONF_FRAME_SIZE_MAX, default=0): cv.positive_int,
            # transfer_max_size : taille max du buffer de control transfer USB.
            # Remplace CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE dans sdkconfig.
            # Valeur recommandée : 2048 pour les caméras avec de gros descripteurs
            # (ex. wTotalLength > 256 bytes).  0 = utiliser la valeur IDF par défaut.
            cv.Optional(CONF_TRANSFER_MAX_SIZE, default=2048): cv.int_range(min=0, max=8192),
            cv.Optional(CONF_ON_STREAM_START): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(UsbUvcStreamStartTrigger)}
            ),
            cv.Optional(CONF_ON_STREAM_STOP): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(UsbUvcStreamStopTrigger)}
            ),
            cv.Optional(CONF_ON_IMAGE): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(UsbUvcImageTrigger)}
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)

# ---------------------------------------------------------------- action schemas
USBUVC_START_STREAM_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(UsbUvcCamera)}
)
USBUVC_STOP_STREAM_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(UsbUvcCamera)}
)

@automation.register_action(
    "usbuvc.start_stream",
    UsbUvcStartStreamAction,
    USBUVC_START_STREAM_ACTION_SCHEMA,
    synchronous=True,
)
async def usbuvc_start_stream_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


USBUVC_CHANGE_FORMAT_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(UsbUvcCamera),
        cv.Required("format"): cv.templatable(cv.string),
    }
)

@automation.register_action(
    "usbuvc.change_format",
    UsbUvcChangeFormatAction,
    USBUVC_CHANGE_FORMAT_ACTION_SCHEMA,
    synchronous=True,
)
async def usbuvc_change_format_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    templ = await cg.templatable(config["format"], args, cg.std_string)
    cg.add(var.set_format(templ))
    return var

@automation.register_action(
    "usbuvc.stop_stream",
    UsbUvcStopStreamAction,
    USBUVC_STOP_STREAM_ACTION_SCHEMA,
    synchronous=True,
)
async def usbuvc_stop_stream_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    cg.add_define("USE_CAMERA")
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_entity(var, config, "camera")
    await cg.register_component(var, config)

    cg.add(var.set_vid(config[CONF_VID]))
    cg.add(var.set_pid(config[CONF_PID]))
    cg.add(var.set_uvc_stream_index(config[CONF_UVC_STREAM_INDEX]))

    w, h = RESOLUTIONS[config["resolution"]]
    cg.add(var.set_resolution(w, h))
    cg.add(var.set_fps(config[CONF_FPS]))

    cg.add(var.set_max_update_interval(int(1000 / config[CONF_MAX_FRAMERATE])))
    if config[CONF_IDLE_FRAMERATE] == 0:
        cg.add(var.set_idle_update_interval(0))
    else:
        cg.add(var.set_idle_update_interval(int(1000 / config[CONF_IDLE_FRAMERATE])))

    cg.add(var.set_frame_buffer_count(config[CONF_FRAME_BUFFER_COUNT]))
    cg.add(var.set_urb_count(config[CONF_URB_COUNT]))
    cg.add(var.set_urb_size(config[CONF_URB_SIZE]))
    cg.add(var.set_frame_size_max(config[CONF_FRAME_SIZE_MAX]))
    # transfer_max_size -> injecté dans sdkconfig via add_idf_sdkconfig_option
    # Evite de devoir mettre CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE manuellement
    if config[CONF_TRANSFER_MAX_SIZE] > 0:
        add_idf_sdkconfig_option(
            "CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE",
            config[CONF_TRANSFER_MAX_SIZE],
        )

    add_idf_component(
        name="espressif/usb_host_uvc",
        ref=">=2.2.0",
    )

    cg.add_platformio_option(
        "build_flags",
        ["-I${PROJECT_DIR}/managed_components/espressif__usb_host_uvc/include"],
    )

    for conf in config.get(CONF_ON_STREAM_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_STREAM_STOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_IMAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(UsbUvcCameraImageData, "image")], conf
        )
