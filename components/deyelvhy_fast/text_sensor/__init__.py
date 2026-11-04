from esphome.components.deyelvhy import text_sensor as shared

# The shared schema and the C++ of the platform live in deyelvhy/text_sensor; auto-loading it builds that C++
AUTO_LOAD = ["text_sensor.deyelvhy"]
DEPENDENCIES = ["deyelvhy"]

CONFIG_SCHEMA = shared.ENTITY_SCHEMA
FINAL_VALIDATE_SCHEMA = shared.ENTITY_FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await shared.build_entities(config, slow=False)
