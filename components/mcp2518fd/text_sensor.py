# text_sensor platform for mcp2518fd
# ESPHome looks for <component>/text_sensor.py for platform: mcp2518fd under text_sensor:
from .canbus import (  # noqa: F401
    TEXT_SENSOR_PLATFORM_SCHEMA as CONFIG_SCHEMA,
    text_sensor_to_code as to_code,
    MCP2518FDCanBusSniffer,
    mcp2518fd_ns,
)
