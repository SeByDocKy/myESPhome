from esphome.components.deyehylv import p_select

DEPENDENCIES = ["deyehylv"]

CONFIG_SCHEMA = p_select.CONFIG_SCHEMA
FINAL_VALIDATE_SCHEMA = p_select.FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await p_select.to_code(config, slow=False)
