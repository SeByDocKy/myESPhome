from esphome.components.deyelvhy import sensor as shared

# The shared schema and the C++ of the platform live in deyelvhy/sensor; auto-loading it builds that C++
AUTO_LOAD = ["sensor.deyelvhy"]
DEPENDENCIES = ["deyelvhy"]

CONFIG_SCHEMA = shared.ENTITY_SCHEMA
FINAL_VALIDATE_SCHEMA = shared.ENTITY_FINAL_VALIDATE_SCHEMA


async def to_code(config):
    await shared.build_entities(config, slow=False)
