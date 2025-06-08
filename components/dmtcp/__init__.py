import esphome.codegen as cg
import esphome.config_validation as cv
# from esphome.components import sensor, output
# from esphome import automation
# from esphome.automation import maybe_simple_id

CODEOWNERS = ["@sebydocky"]
MULTI_CONF = True

dmtcp_ns = cg.esphome_ns.namespace("dmtcp")
DMTCPComponent = dmtcp_ns.class_("DMTCPComponent", cg.PollingComponent)

from esphome.const import (
    CONF_ID,
)
CONF_DMTCP_ID = "dmtcp_id"
CONF_HOST_IP_ADDRESS = 'host_ip_address'
CONF_HOST_PORT = 'host_port'
CONF_UNIT_ID = 'unit_id'


DMTCPComponent_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DMTCP_ID): cv.use_id(DMTCPComponent),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
          cv.GenerateID(): cv.declare_id(DMTCPComponent),
          cv.Required(CONF_HOST_IP_ADDRESS): cv.templatable(cv.string),
          cv.Optional(CONF_HOST_PORT, default=8899): cv.int_range(min=1, max=65535),
          cv.Optional(CONF_UNIT_ID, default=1): cv.int_range(min=1, max=255),          
        }
    ).extend(cv.polling_component_schema("60s"))
 )
 
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_host_ip_address(config[CONF_HOST_IP_ADDRESS]))
    
    if CONF_HOST_PORT in config:
        cg.add(var.set_host_port(config[CONF_HOST_PORT]))
        
    if CONF_UNIT_ID in config:
        cg.add(var.set_unit_id(config[CONF_UNIT_ID]))
	