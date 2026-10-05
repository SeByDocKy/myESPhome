import esphome.codegen as cg
from esphome.components import vecan
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["vecan"]
MULTI_CONF = True

CONF_SMARTSOLAR_ID = "smartsolar_id"
CONF_VECAN_ID = "vecan_id"
CONF_ADDRESS = "address"
CONF_POLL_INTERVAL = "poll_interval"
CONF_BATTERY_INSTANCE = "battery_instance"
CONF_PV_INSTANCE = "pv_instance"

smartsolar_ns = cg.esphome_ns.namespace("smartsolar")
SmartSolar = smartsolar_ns.class_("SmartSolar", cg.PollingComponent, vecan.VeCanDevice)


def _validate_instances(config):
    if config[CONF_BATTERY_INSTANCE] == config[CONF_PV_INSTANCE]:
        raise cv.Invalid(
            f"{CONF_BATTERY_INSTANCE} and {CONF_PV_INSTANCE} must be different"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SmartSolar),
            cv.Required(CONF_VECAN_ID): cv.use_id(vecan.VeCanHub),
            # Source address of the charger on the VE.Can bus. The vecan hub logs the Victron devices it discovers.
            cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=253),
            # How often the registers (yield, state, ...) are requested. The live values are broadcast by the charger.
            cv.Optional(CONF_POLL_INTERVAL, default="10s"): cv.positive_time_period_milliseconds,
            # NMEA 2000 "battery instance" carrying the battery side / the PV side in PGN 127508
            cv.Optional(CONF_BATTERY_INSTANCE, default=0): cv.int_range(min=0, max=252),
            cv.Optional(CONF_PV_INSTANCE, default=1): cv.int_range(min=0, max=252),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_instances,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_VECAN_ID])
    cg.add(var.set_vecan(hub))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    # register_component() only wires "update_interval", so the renamed key needs an explicit call
    cg.add(var.set_update_interval(config[CONF_POLL_INTERVAL]))
    cg.add(var.set_battery_instance(config[CONF_BATTERY_INSTANCE]))
    cg.add(var.set_pv_instance(config[CONF_PV_INSTANCE]))
