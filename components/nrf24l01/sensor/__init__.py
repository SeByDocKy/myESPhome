import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from .. import NRF24Component, nrf24l01_ns

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["nrf24l01"]

CONF_NRF24L01_ID = "nrf24l01_id"
CONF_DUTY_CYCLE = "duty_cycle"
CONF_HM_COUNT = "hm_count"

NRF24DutyCycleSensor = nrf24l01_ns.class_(
    "NRF24DutyCycleSensor", sensor.Sensor, cg.PollingComponent
)

_DUTY_CYCLE_SCHEMA = sensor.sensor_schema(
    NRF24DutyCycleSensor,
    unit_of_measurement=UNIT_PERCENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
    icon="mdi:radio-tower",
).extend(cv.polling_component_schema("60s"))

# hm_count est un compteur événementiel (publié par nrf24l01: à chaque changement
# d'état reachable d'un hm:, pas de scrutation) -- un sensor::Sensor de base suffit.
_HM_COUNT_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    icon="mdi:numeric",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NRF24L01_ID): cv.use_id(NRF24Component),
        cv.Optional(CONF_DUTY_CYCLE): _DUTY_CYCLE_SCHEMA,
        cv.Optional(CONF_HM_COUNT): _HM_COUNT_SCHEMA,
    }
)


def _final_validate(config):
    if CONF_HM_COUNT not in config:
        return config

    # hm_count n'a de sens que s'il existe au moins un hm: rattaché à ce
    # nrf24l01: -- sinon le compteur resterait toujours à 0.
    try:
        full_conf = fv.full_config.get()
        hm_confs = full_conf.get("hm", [])
        if isinstance(hm_confs, dict):
            hm_confs = [hm_confs]
    except Exception:  # noqa: BLE001
        return config

    radio_id = config[CONF_NRF24L01_ID]
    if not any(hm_conf.get("nrf24l01_id") == radio_id for hm_conf in hm_confs):
        raise cv.Invalid(
            f"'hm_count' nécessite qu'au moins un composant hm: soit rattaché à "
            f"ce nrf24l01_id ('{radio_id}') -- sans hm:, ce compteur resterait "
            f"toujours à 0."
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    radio = await cg.get_variable(config[CONF_NRF24L01_ID])

    if CONF_DUTY_CYCLE in config:
        conf = config[CONF_DUTY_CYCLE]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(radio))

    if CONF_HM_COUNT in config:
        s = await sensor.new_sensor(config[CONF_HM_COUNT])
        cg.add(radio.set_hm_count_sensor(s))
