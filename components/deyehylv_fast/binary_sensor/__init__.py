from esphome.components.deyehylv import p_binary_sensor

DEPENDENCIES = ["deyehylv"]

CONFIG_SCHEMA = p_binary_sensor.CONFIG_SCHEMA
FINAL_VALIDATE_SCHEMA = p_binary_sensor.FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await p_binary_sensor.to_code(config, slow=False)
