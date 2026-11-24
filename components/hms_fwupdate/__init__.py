import re

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import http_request
from esphome.components import hms as hms_module
from esphome.const import CONF_ID, CONF_NAME, CONF_TRIGGER_ID, CONF_URL

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["hms", "http_request"]
AUTO_LOAD = ["sha256"]
MULTI_CONF = True

hms_fwupdate_ns = cg.esphome_ns.namespace("hms_fwupdate")
HMSFwUpdateComponent = hms_fwupdate_ns.class_("HMSFwUpdateComponent", cg.Component)

ProgressTrigger = hms_fwupdate_ns.class_("ProgressTrigger", automation.Trigger.template(cg.float_))
ResultTrigger = hms_fwupdate_ns.class_("ResultTrigger", automation.Trigger.template(cg.std_string))

StartAction = hms_fwupdate_ns.class_("StartAction", automation.Action)
AbortAction = hms_fwupdate_ns.class_("AbortAction", automation.Action)
ArmAction = hms_fwupdate_ns.class_("ArmAction", automation.Action)

CONF_HMS_ID = "hms_id"
CONF_FIRMWARES = "firmwares"
CONF_SHA256 = "sha256"
CONF_MIN_LINK_QUALITY = "min_link_quality"
CONF_LINK_QUALITY_WINDOW = "link_quality_window"
CONF_REQUIRE_ARMED = "require_armed"
CONF_ARMED_TIMEOUT = "armed_timeout"
CONF_DRY_RUN = "dry_run"
CONF_MAX_ROW_ATTEMPTS = "max_row_attempts"
CONF_STRICT_HEX = "strict_hex"
CONF_ON_PROGRESS = "on_progress"
CONF_ON_RESULT = "on_result"

_SHA256_RE = re.compile(r"^[0-9A-Fa-f]{64}$")


def _validate_sha256(value):
    value = cv.string_strict(value)
    if not _SHA256_RE.match(value):
        raise cv.Invalid("sha256 must be 64 hexadecimal characters")
    return value.lower()


def _validate_unique_names(firmwares):
    names = [fw[CONF_NAME] for fw in firmwares]
    if len(set(names)) != len(names):
        raise cv.Invalid("The firmware names must be unique")
    return firmwares


FIRMWARE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Required(CONF_URL): cv.url,
        cv.Optional(CONF_SHA256): _validate_sha256,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HMSFwUpdateComponent),
        cv.Required(CONF_HMS_ID): cv.use_id(hms_module.HMSComponent),
        cv.GenerateID(http_request.CONF_HTTP_REQUEST_ID): cv.use_id(
            http_request.HttpRequestComponent
        ),
        cv.Required(CONF_FIRMWARES): cv.All(
            cv.ensure_list(FIRMWARE_SCHEMA), cv.Length(min=1), _validate_unique_names
        ),
        # The update is refused below this share of successful telemetry polls (over the last
        # `link_quality_window` polls): a weak link makes the update slow and risky.
        cv.Optional(CONF_MIN_LINK_QUALITY, default="90%"): cv.percentage,
        cv.Optional(CONF_LINK_QUALITY_WINDOW, default=20): cv.int_range(min=5, max=32),
        # `start` only works after the armed switch was turned on (it turns itself off again).
        cv.Optional(CONF_REQUIRE_ARMED, default=True): cv.boolean,
        cv.Optional(CONF_ARMED_TIMEOUT, default="60s"): cv.positive_time_period_milliseconds,
        # Verify the file and log the frames, but never transmit. Set to false to really flash.
        cv.Optional(CONF_DRY_RUN, default=True): cv.boolean,
        # Whole-row re-sends before giving up (the reference implementation uses 10).
        cv.Optional(CONF_MAX_ROW_ATTEMPTS, default=10): cv.int_range(min=1, max=30),
        # Verify Intel HEX checksums and lengths. Leave on unless a genuine file is refused.
        cv.Optional(CONF_STRICT_HEX, default=True): cv.boolean,
        cv.Optional(CONF_ON_PROGRESS): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ProgressTrigger)}
        ),
        cv.Optional(CONF_ON_RESULT): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ResultTrigger)}
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_HMS_ID])
    cg.add(var.set_hub(hub))
    http = await cg.get_variable(config[http_request.CONF_HTTP_REQUEST_ID])
    cg.add(var.set_http(http))

    for fw in config[CONF_FIRMWARES]:
        cg.add(var.add_firmware(fw[CONF_NAME], fw[CONF_URL], fw.get(CONF_SHA256, "")))

    cg.add(var.set_min_link_quality(int(round(config[CONF_MIN_LINK_QUALITY] * 100))))
    cg.add(var.set_link_quality_window(config[CONF_LINK_QUALITY_WINDOW]))
    cg.add(var.set_require_armed(config[CONF_REQUIRE_ARMED]))
    cg.add(var.set_armed_timeout(config[CONF_ARMED_TIMEOUT]))
    cg.add(var.set_dry_run(config[CONF_DRY_RUN]))
    cg.add(var.set_max_row_attempts(config[CONF_MAX_ROW_ATTEMPTS]))
    cg.add(var.set_strict_hex(config[CONF_STRICT_HEX]))

    for conf in config.get(CONF_ON_PROGRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.float_, "x")], conf)
    for conf in config.get(CONF_ON_RESULT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)


_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(HMSFwUpdateComponent)}
)


@automation.register_action("hms_fwupdate.start", StartAction, _ACTION_SCHEMA)
@automation.register_action("hms_fwupdate.abort", AbortAction, _ACTION_SCHEMA)
@automation.register_action("hms_fwupdate.arm", ArmAction, _ACTION_SCHEMA)
async def hms_fwupdate_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
