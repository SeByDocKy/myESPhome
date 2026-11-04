from esphome.components.deyehylv import p_text_sensor

DEPENDENCIES = ["deyehylv"]

CONFIG_SCHEMA = p_text_sensor.CONFIG_SCHEMA
FINAL_VALIDATE_SCHEMA = p_text_sensor.FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await p_text_sensor.to_code(config, slow=True)
