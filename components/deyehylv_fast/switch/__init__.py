from esphome.components.deyehylv import p_switch

DEPENDENCIES = ["deyehylv"]

CONFIG_SCHEMA = p_switch.CONFIG_SCHEMA
FINAL_VALIDATE_SCHEMA = p_switch.FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await p_switch.to_code(config, slow=False)
