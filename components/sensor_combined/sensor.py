import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

sensor_combined_ns = cg.esphome_ns.namespace("sensor_combined")
SensorCombined = sensor_combined_ns.class_(
    "SensorCombined", sensor.Sensor, cg.Component
)

CONF_OPERATION = "operation"
CONF_SENSORS = "sensors"
CONF_PROPAGATE_NAN = "propagate_nan"

# Matches the CombineOperation enum in sensor_combined.h.
# cv.enum() emits a plain integer literal, so the C++ setter takes an
# int and does the static_cast internally (see set_operation()).
OPERATIONS = {
    "sum": 0,
    "mean": 1,
    "prod": 2,
}

CONFIG_SCHEMA = sensor.sensor_schema(
    SensorCombined,
    accuracy_decimals=2,
).extend(
    {
        cv.Required(CONF_OPERATION): cv.enum(OPERATIONS, lower=True),
        cv.Required(CONF_SENSORS): cv.All(
            cv.ensure_list(cv.use_id(sensor.Sensor)), cv.Length(min=1)
        ),
        # False (default): a NaN source sensor is skipped, the
        # operation runs on the remaining valid values.
        # True: any NaN source sensor makes the whole result NaN.
        cv.Optional(CONF_PROPAGATE_NAN, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_operation(config[CONF_OPERATION]))
    cg.add(var.set_propagate_nan(config[CONF_PROPAGATE_NAN]))

    # Register every source sensor. No explicit count is needed: the
    # list length in YAML drives the loop.
    for sensor_id in config[CONF_SENSORS]:
        sens = await cg.get_variable(sensor_id)
        cg.add(var.add_sensor(sens))
