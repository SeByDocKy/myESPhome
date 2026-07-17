import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import RYLR999BLEComponent, CONF_RYLR999_BLE_ID

DEPENDENCIES = ["rylr999_ble"]

CONF_LORA_READY = "lora_ready"

# Schéma en "slots nommés" à l'intérieur d'une seule entrée de plateforme
# (comme bme280/sen5x), pour pouvoir ajouter d'autres indicateurs binaires du
# module plus tard sans casser la config existante.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RYLR999_BLE_ID): cv.use_id(RYLR999BLEComponent),
        cv.Optional(CONF_LORA_READY): binary_sensor.binary_sensor_schema(),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RYLR999_BLE_ID])

    if lora_ready_config := config.get(CONF_LORA_READY):
        sens = await binary_sensor.new_binary_sensor(lora_ready_config)
        cg.add(parent.set_lora_ready_binary_sensor(sens))
