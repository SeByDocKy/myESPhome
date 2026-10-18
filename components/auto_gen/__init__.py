"""auto_gen: prototype that registers ESPHome entities at runtime.

Why this component exists
-------------------------
ESPHome stores every entity in fixed-size containers (``StaticVector``) whose
size is computed at compile time from the YAML.  Registering an entity after
boot therefore needs two things:

1. extra *slots* reserved at code generation time  (done here, see ``to_code``),
2. the entity strings (unit, ...) pre-registered in the generated tables
   (done here with ``register_unit_of_measurement``).

The component then creates a ``sensor::Sensor`` in C++ after a delay,
registers it, publishes values, and fires ``on_registered`` so that an
automation (for example the ``api_resync`` component) can ask Home Assistant
to re-read the entity list.

NOTE: this relies on ``esphome.core.entity_helpers`` which is an internal
ESPHome module (packed entity fields, ESPHome >= 2026.8).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_UNIT_OF_MEASUREMENT
from esphome.core import CORE
from esphome.core.entity_helpers import register_unit_of_measurement

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["sensor"]

CONF_DELAY = "delay"
CONF_COUNT = "count"
CONF_CREATE_INTERVAL = "create_interval"
CONF_MAX_SENSORS = "max_sensors"
CONF_SENSOR_NAME = "sensor_name"
CONF_ON_REGISTERED = "on_registered"

auto_gen_ns = cg.esphome_ns.namespace("auto_gen")
AutoGenComponent = auto_gen_ns.class_("AutoGenComponent", cg.Component)
RegisteredTrigger = auto_gen_ns.class_(
    "RegisteredTrigger", automation.Trigger.template()
)

def _validate_count(config):
    if config[CONF_COUNT] > config[CONF_MAX_SENSORS]:
        raise cv.Invalid(
            f"'{CONF_COUNT}' ({config[CONF_COUNT]}) cannot exceed "
            f"'{CONF_MAX_SENSORS}' ({config[CONF_MAX_SENSORS]}): "
            "only the reserved slots can be used at runtime"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AutoGenComponent),
            # Delay between boot and the creation of the first demo sensor.
            cv.Optional(
                CONF_DELAY, default="10s"
            ): cv.positive_time_period_milliseconds,
            # Number of demo sensors created at runtime, one every create_interval.
            cv.Optional(CONF_COUNT, default=1): cv.int_range(min=1, max=64),
            # Delay between two successive creations.
            cv.Optional(
                CONF_CREATE_INTERVAL, default="15s"
            ): cv.positive_time_period_milliseconds,
            # Number of sensor slots reserved at compile time for runtime creation.
            cv.Optional(CONF_MAX_SENSORS, default=4): cv.int_range(min=1, max=64),
            # Base name of the demo sensors (a _<n> suffix is added when count > 1).
            cv.Optional(CONF_SENSOR_NAME, default="auto_gen_test"): cv.string,
            # Unit of the demo sensors (must be known at compile time).
            cv.Optional(CONF_UNIT_OF_MEASUREMENT, default="W"): cv.string,
            cv.Optional(CONF_ON_REGISTERED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RegisteredTrigger),
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_count,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_delay(config[CONF_DELAY]))
    cg.add(var.set_count(config[CONF_COUNT]))
    cg.add(var.set_create_interval(config[CONF_CREATE_INTERVAL]))
    cg.add(var.set_max_sensors(config[CONF_MAX_SENSORS]))
    cg.add(var.set_sensor_name(config[CONF_SENSOR_NAME]))

    # Reserve extra sensor slots: ESPHOME_ENTITY_SENSOR_COUNT is generated from
    # CORE.platform_counts at CoroPriority.FINAL, i.e. after this to_code().
    # This also guarantees that USE_SENSOR is defined.
    CORE.platform_counts["sensor"] += config[CONF_MAX_SENSORS]

    # The unit string must live in the generated PROGMEM table; only its index
    # is passed to C++.
    uom_index = register_unit_of_measurement(config[CONF_UNIT_OF_MEASUREMENT])
    cg.add(var.set_uom_index(uom_index))

    for conf in config.get(CONF_ON_REGISTERED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
