import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["canbus"]
MULTI_CONF = True

pcm3k6w_ns = cg.esphome_ns.namespace("pcm3k6w")
PCM3K6WComponent = pcm3k6w_ns.class_("PCM3K6WComponent", cg.Component)

CONF_PCM3K6W_ID = "pcm3k6w_id"
CONF_CANBUS_ID = "canbus_id"
CONF_ADDRESS = "address"
CONF_POLL_INTERVAL = "poll_interval"
CONF_FRAME_GAP = "frame_gap"


def validate_address(value):
    """Accept either a plain int (0x01, 1, ...) or a bare hex-digit string
    like '01' / '0A' (no 0x prefix), matching how the PCM module address is
    written elsewhere (e.g. the original YAML's `${pcm_address}` substitution)."""
    if isinstance(value, int):
        if not 0 <= value <= 0xFF:
            raise cv.Invalid("Address must be between 0x00 and 0xFF")
        return value
    value = cv.string_strict(value)
    try:
        parsed = int(value, 16)
    except ValueError as exc:
        raise cv.Invalid(
            f"Invalid address '{value}' - expected a hex string like '01' or '0A', or an integer"
        ) from exc
    if not 0 <= parsed <= 0xFF:
        raise cv.Invalid("Address must be between 0x00 and 0xFF")
    return parsed


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PCM3K6WComponent),
        cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        cv.Optional(CONF_ADDRESS, default="01"): validate_address,
        cv.Optional(CONF_POLL_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_FRAME_GAP, default="100ms"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    canbus_component = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(canbus_component))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_poll_interval(config[CONF_POLL_INTERVAL]))
    cg.add(var.set_frame_gap(config[CONF_FRAME_GAP]))


# Shared by every platform (sensor/binary_sensor/switch/number/select): each
# `platform: pcm3k6w` block declares `pcm3k6w_id:` pointing at the hub
# declared in the top-level `pcm3k6w:` block.
PCM3K6W_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PCM3K6W_ID): cv.use_id(PCM3K6WComponent),
    }
)
