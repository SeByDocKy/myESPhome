from esphome.components.deyehylv import binary_sensor as shared

# The shared schema and the C++ of the platform live in deyehylv/binary_sensor; auto-loading it builds that C++
AUTO_LOAD = ["binary_sensor.deyehylv"]
DEPENDENCIES = ["deyehylv"]

CONFIG_SCHEMA = shared.ENTITY_SCHEMA
FINAL_VALIDATE_SCHEMA = shared.ENTITY_FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await shared.build_entities(config, slow=False)
