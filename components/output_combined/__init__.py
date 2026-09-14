import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output

CODEOWNERS = ["@SeByDocKy"]

# Namespace and class for the native C++ implementation
output_combined_ns = cg.esphome_ns.namespace("output_combined")
OutputCombined = output_combined_ns.class_(
    "OutputCombined", output.FloatOutput, cg.Component
)

CONF_OUTPUTS = "outputs"

# Reuse the standard float output schema (min_power, max_power, zero_means_zero...)
# and add a required list of at least one sub-output to drive.
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(OutputCombined),
        cv.Required(CONF_OUTPUTS): cv.All(
            cv.ensure_list(cv.use_id(output.FloatOutput)), cv.Length(min=1)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[cv.CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    # Register every sub-output listed in the "outputs" list.
    # No explicit count is needed: the list length drives the loop.
    for out_id in config[CONF_OUTPUTS]:
        out = await cg.get_variable(out_id)
        cg.add(var.add_output(out))
