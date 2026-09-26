"""Native ESPHome component for Zendure SolarFlow batteries over their local zenSDK HTTP API.

Protocol reference: https://github.com/Zendure/zenSDK (README + docs/en_properties.md)
Behavioural reference (command sequences, device limits): https://github.com/Zendure/Zendure-HA
(custom_components/zendure_ha/device.py, class ZendureZenSdk).

The device exposes a small REST API on its own IP:
  GET  /properties/report   -> {"properties": {...}, "packData": [{...}, ...]}
  POST /properties/write    -> {"sn": "<serial>", "id": <n>, "properties": {...}}

Supported models are the zenSDK ones: SolarFlow 800 / 800 Plus / 800 Pro, 1600 AC+,
2400 AC / AC+ / Pro. Legacy MQTT-only devices (Hyper 2000, Hub 1200/2000, Ace 1500,
AIO 2400) are NOT supported.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL, CONF_PORT

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["json"]
# One `zensdk:` block per battery/hub (each opens its own HTTP connection):
#   zensdk:
#     - id: zensdk_1
#       host: 192.168.1.60
#       sn: "WOB1NHMAMXXXXX3"
#     - id: zensdk_2
#       host: 192.168.1.61
#       sn: "WOB1NHMAMXXXXX4"
MULTI_CONF = True

zensdk_ns = cg.esphome_ns.namespace("zensdk")
ZenSdkComponent = zensdk_ns.class_("ZenSdkComponent", cg.PollingComponent)

CONF_ZENSDK_ID = "zensdk_id"
CONF_HOST = "host"
CONF_SN = "sn"
CONF_MAX_CHARGE_POWER = "max_charge_power"
CONF_MAX_DISCHARGE_POWER = "max_discharge_power"
CONF_SOC_SCALE = "soc_scale"
CONF_WRITE_FLASH_ON_STOP = "write_flash_on_stop"
# Named "poll_interval" (not "update_interval") to match this author's hmsw / tsungen3
# components.
CONF_POLL_INTERVAL = "poll_interval"

# Number of battery packs (packData entries) for which per-pack entities can be
# configured. Shared by the sensor / text_sensor platforms.
MAX_PACKS = 6

# (max charge W, max discharge W) -- taken from Zendure-HA's devices/solarflow*.py setLimits().
MODEL_LIMITS = {
    "SF800": (1000, 800),
    "SF800_PLUS": (1000, 800),
    "SF800_PRO": (1000, 800),
    "SF1600_AC_PLUS": (1600, 1600),
    "SF2400_AC": (2400, 2400),
    "SF2400_AC_PLUS": (3200, 2400),
    "SF2400_PRO": (3200, 2400),
}


def _apply_model_defaults(config):
    """Fill max_charge_power / max_discharge_power from `model` when not given explicitly."""
    config = dict(config)
    charge, discharge = MODEL_LIMITS.get(config.get(CONF_MODEL), (None, None))
    for key, preset in ((CONF_MAX_CHARGE_POWER, charge), (CONF_MAX_DISCHARGE_POWER, discharge)):
        if key not in config:
            if preset is None:
                raise cv.Invalid(f"'{key}' is required when '{CONF_MODEL}' is not set")
            config[key] = preset
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZenSdkComponent),
            # IP address recommended (the device also announces itself via mDNS as
            # `Zendure-<Model>-<Last12MAC>`, but ESPHome cannot resolve .local names).
            cv.Required(CONF_HOST): cv.string_strict,
            cv.Optional(CONF_PORT, default=80): cv.port,
            # Device serial number: mandatory in every POST body of the local API.
            cv.Required(CONF_SN): cv.string_strict,
            cv.Optional(CONF_MODEL): cv.one_of(*MODEL_LIMITS, upper=True),
            cv.Optional(CONF_MAX_CHARGE_POWER): cv.int_range(min=0, max=10000),
            cv.Optional(CONF_MAX_DISCHARGE_POWER): cv.int_range(min=0, max=10000),
            # Raw socSet / minSoc units per percent. Zendure-HA scales these by 10 (raw 1000 = 100 %)
            # while zenSDK's property table documents plain percent -- check your firmware, see README.
            cv.Optional(CONF_SOC_SCALE, default=10): cv.one_of(1, 10, int=True),
            # Stop command: smartMode 1 (default, nothing written to flash) or smartMode 0
            # (as the Home Assistant integration does when not off-grid: persists the zero limits).
            cv.Optional(CONF_WRITE_FLASH_ON_STOP, default=False): cv.boolean,
            cv.Optional(CONF_POLL_INTERVAL, default="10s"): cv.update_interval,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _apply_model_defaults,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_sn(config[CONF_SN]))
    cg.add(var.set_max_charge_power(config[CONF_MAX_CHARGE_POWER]))
    cg.add(var.set_max_discharge_power(config[CONF_MAX_DISCHARGE_POWER]))
    cg.add(var.set_soc_scale(config[CONF_SOC_SCALE]))
    cg.add(var.set_write_flash_on_stop(config[CONF_WRITE_FLASH_ON_STOP]))
    # register_component() only auto-wires PollingComponent's interval when the config key is
    # literally "update_interval" -- ours is "poll_interval", so it is set explicitly here.
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL]))
