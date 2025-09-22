import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_JK_BMS_ID, JK_BMS_COMPONENT_SCHEMA
from .const import CONF_CHARGING, CONF_DISCHARGING

DEPENDENCIES = ["jk_bms"]

CODEOWNERS = ["@syssi"]

CONF_CHARGING_SWITCH = (
    "charging_switch"  # @DEPRECATED and superseded by switch.charging
)
CONF_DISCHARGING_SWITCH = (
    "discharging_switch"  # @DEPRECATED and superseded by switch.discharging
)
CONF_BALANCING = "balancing"
CONF_BALANCING_SWITCH = "balancing_switch"
CONF_DEDICATED_CHARGER_SWITCH = "dedicated_charger_switch"
CONF_ONLINE_STATUS = "online_status"

ICON_CHARGING = "mdi:battery-charging"
ICON_CHARGING_SWITCH = "mdi:battery-charging"
ICON_DISCHARGING = "mdi:power-plug"
ICON_DISCHARGING_SWITCH = "mdi:power-plug"
ICON_BALANCING = "mdi:battery-heart-variant"
ICON_BALANCING_SWITCH = "mdi:battery-heart-variant"
ICON_DEDICATED_CHARGER_SWITCH = "mdi:battery-charging"

BINARY_SENSORS = [
    CONF_CHARGING,
    CONF_CHARGING_SWITCH,
    CONF_DISCHARGING,
    CONF_DISCHARGING_SWITCH,
    CONF_BALANCING,
    CONF_BALANCING_SWITCH,
    CONF_DEDICATED_CHARGER_SWITCH,
    CONF_ONLINE_STATUS,
]

CONFIG_SCHEMA = JK_BMS_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(
            icon=ICON_CHARGING
        ),
        cv.Optional(CONF_CHARGING_SWITCH): binary_sensor.binary_sensor_schema(
            icon=ICON_CHARGING_SWITCH
        ),
        cv.Optional(CONF_DISCHARGING): binary_sensor.binary_sensor_schema(
            icon=ICON_DISCHARGING
        ),
        cv.Optional(CONF_DISCHARGING_SWITCH): binary_sensor.binary_sensor_schema(
            icon=ICON_DISCHARGING_SWITCH
        ),
        cv.Optional(CONF_BALANCING): binary_sensor.binary_sensor_schema(
            icon=ICON_BALANCING
        ),
        cv.Optional(CONF_BALANCING_SWITCH): binary_sensor.binary_sensor_schema(
            icon=ICON_BALANCING_SWITCH
        ),
        cv.Optional(CONF_DEDICATED_CHARGER_SWITCH): binary_sensor.binary_sensor_schema(
            icon=ICON_DEDICATED_CHARGER_SWITCH
        ),
        cv.Optional(CONF_ONLINE_STATUS): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_JK_BMS_ID])
    for key in BINARY_SENSORS:
        if key in config:
            conf = config[key]
            sens = cg.new_Pvariable(conf[CONF_ID])
            await binary_sensor.register_binary_sensor(sens, conf)
            cg.add(getattr(hub, f"set_{key}_binary_sensor")(sens))
