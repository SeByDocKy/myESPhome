from esphome.components.deyehylv import p_number

DEPENDENCIES = ["deyehylv"]

CONFIG_SCHEMA = p_number.CONFIG_SCHEMA
FINAL_VALIDATE_SCHEMA = p_number.FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await p_number.to_code(config, slow=True)
