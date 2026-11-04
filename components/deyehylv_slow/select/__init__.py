from esphome.components.deyehylv import select as shared

# The shared schema and the C++ of the platform live in deyehylv/select; auto-loading it builds that C++
AUTO_LOAD = ["select.deyehylv"]
DEPENDENCIES = ["deyehylv"]

CONFIG_SCHEMA = shared.ENTITY_SCHEMA
FINAL_VALIDATE_SCHEMA = shared.ENTITY_FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await shared.build_entities(config, slow=True)
