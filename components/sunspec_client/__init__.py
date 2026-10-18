"""Native ESPHome client for SunSpec devices over Modbus (RTU or TCP).

SunSpec devices (inverters, batteries, meters, ...) describe themselves: after a
"SunS" marker, the device exposes a chain of *models*, each introduced by a
two-register header (model id, length), ended by the id 0xFFFF.

This component is a client on the core ``modbus`` bus. The transport is chosen
by the bus ``uart_id``: a real UART (RS485) or ``tcp_uart`` + ``modbus_tcp_uart``
for Modbus TCP, exactly like the ``anker_solix`` component.

Milestone 1 (this version): scan the models exposed by the device and publish
the list in a ``text_sensor`` platform. ``models`` and ``name_prefix`` are
accepted and logged already, they are used by the entity creation that follows
(entities are created once the scan is complete, then Home Assistant is asked
to re-read the entity list).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import modbus
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@SeByDocKy"]
AUTO_LOAD = ["modbus"]
# Several devices on the same bus (or several buses) are possible: one block per device.
MULTI_CONF = True

sunspec_client_ns = cg.esphome_ns.namespace("sunspec_client")
SunSpecClient = sunspec_client_ns.class_(
    "SunSpecClient", cg.Component, modbus.ModbusClientDevice
)

CONF_SUNSPEC_CLIENT_ID = "sunspec_client_id"
CONF_MODELS = "models"
CONF_NAME_PREFIX = "name_prefix"
CONF_STARTUP_DELAY = "startup_delay"
CONF_SCAN_RETRY_INTERVAL = "scan_retry_interval"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SunSpecClient),
            # Wish list of SunSpec model ids (compile time). The entities created after
            # the scan are the intersection of this list and what the device exposes.
            cv.Optional(CONF_MODELS, default=[]): cv.ensure_list(
                cv.int_range(min=1, max=65534)
            ),
            # Prefix of the name of every entity created by this component.
            cv.Optional(CONF_NAME_PREFIX, default=""): cv.string,
            # Wait before the first request, so that the link (WiFi / TCP) is up.
            cv.Optional(
                CONF_STARTUP_DELAY, default="5s"
            ): cv.positive_time_period_milliseconds,
            # Wait before scanning again after a failed scan.
            cv.Optional(
                CONF_SCAN_RETRY_INTERVAL, default="30s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(modbus.modbus_device_schema(0x01))
)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("sunspec_client", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Sets the parent bus and the unit address (the class is itself a Modbus client device).
    await modbus.register_modbus_client_device(var, config)

    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY]))
    cg.add(var.set_scan_retry_interval(config[CONF_SCAN_RETRY_INTERVAL]))
    cg.add(var.set_name_prefix(config[CONF_NAME_PREFIX]))
    for model in config[CONF_MODELS]:
        cg.add(var.add_wanted_model(model))
