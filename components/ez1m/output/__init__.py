import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import output
from esphome.const import CONF_ID, CONF_MODEL
from .. import ez1m_ns, EZ1MComponent, CONF_EZ1M_ID, MODEL_MAX_WATTS

DEPENDENCIES = ["ez1m"]

EZ1MOutput = ez1m_ns.class_(
    "EZ1MOutput", output.FloatOutput, cg.Parented.template(EZ1MComponent)
)

CONF_POWER_OUTPUT = "power_output"
CONF_MAX_POWER = "max_power"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EZ1M_ID): cv.use_id(EZ1MComponent),
        cv.Optional(CONF_POWER_OUTPUT): output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(EZ1MOutput),
                # No static default: the real default is the hub's
                # model-dependent max power (800/960/1800 W for
                # ez1m/ez1h/ez1d), filled in by _final_validate below once the
                # referenced hub's `model` is known. Set this explicitly in
                # YAML to override either way.
                cv.Optional(CONF_MAX_POWER): cv.float_,
            }
        ),
    }
)


def _final_validate(config):
    # Fill in power_output's max_power from the referenced hub's `model`,
    # unless the user already set it explicitly in YAML. Mirrors number/
    # __init__.py's handling of power_limit's max_value.
    if CONF_POWER_OUTPUT not in config:
        return config
    po_conf = config[CONF_POWER_OUTPUT]
    if CONF_MAX_POWER in po_conf:
        return config

    fconf = fv.full_config.get()
    hub_path = fconf.get_path_for_id(config[CONF_EZ1M_ID])[:-1]
    hub_conf = fconf.get_config_for_path(hub_path)
    model = hub_conf.get(CONF_MODEL, "ez1m")
    po_conf[CONF_MAX_POWER] = MODEL_MAX_WATTS[model]
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EZ1M_ID])
    if CONF_POWER_OUTPUT in config:
        conf = config[CONF_POWER_OUTPUT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await output.register_output(var, conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_max_power(conf[CONF_MAX_POWER]))
