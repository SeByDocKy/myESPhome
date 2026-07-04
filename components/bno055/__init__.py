import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_BNO055_ID = "bno055_id"
CONF_OPERATION_MODE = "operation_mode"
CONF_USE_EXTERNAL_CRYSTAL = "use_external_crystal"
CONF_SAVE_CALIBRATION = "save_calibration"
CONF_RESTORE_CALIBRATION = "restore_calibration"
CONF_AXIS_REMAP = "axis_remap"
CONF_X_AXIS = "x_axis"
CONF_Y_AXIS = "y_axis"
CONF_Z_AXIS = "z_axis"
CONF_X_SIGN = "x_sign"
CONF_Y_SIGN = "y_sign"
CONF_Z_SIGN = "z_sign"

bno055_ns = cg.esphome_ns.namespace("bno055")
BNO055Component = bno055_ns.class_(
    "BNO055Component", cg.PollingComponent, i2c.I2CDevice
)

# Must mirror the OperationMode enum class declared in bno055.h.
OperationMode = bno055_ns.enum("OperationMode", is_class=True)

# Only the "useful" fusion / hybrid modes are exposed. CONFIG and the
# single/dual raw-sensor modes (ACCONLY, MAGONLY, ...) are intentionally
# left out since this component is meant to publish sensor-fusion data;
# add them to this map later if raw-only operation is ever needed.
OPERATION_MODES = {
    "IMU": OperationMode.MODE_IMUPLUS,  # accel + gyro fusion, no mag (fast, no heading)
    "COMPASS": OperationMode.MODE_COMPASS,  # accel + mag, absolute heading, no gyro
    "M4G": OperationMode.MODE_M4G,  # accel + mag, relative heading
    "NDOF_FMC_OFF": OperationMode.MODE_NDOF_FMC_OFF,  # 9-DOF fusion, fast magnetometer calibration off
    "NDOF": OperationMode.MODE_NDOF,  # full 9-DOF absolute orientation fusion (default, most common)
}

# AXIS_MAP_CONFIG register (0x41) source-axis encoding (datasheet §4.3.61):
# 2 bits per output axis, value = which physical axis feeds it.
AXIS_SOURCE = {"X": 0b00, "Y": 0b01, "Z": 0b10}
# AXIS_MAP_SIGN register (0x42): one bit per axis, 0 = positive, 1 = negative.
AXIS_SIGN = {"POSITIVE": 0, "NEGATIVE": 1}


def _validate_axis_remap(config):
    axes = [config[CONF_X_AXIS], config[CONF_Y_AXIS], config[CONF_Z_AXIS]]
    if sorted(axes) != sorted(AXIS_SOURCE.values()):
        raise cv.Invalid(
            "x_axis, y_axis and z_axis must together reference each of X, Y and Z "
            "exactly once"
        )
    return config


AXIS_REMAP_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_X_AXIS, default="X"): cv.enum(AXIS_SOURCE, upper=True),
            cv.Optional(CONF_Y_AXIS, default="Y"): cv.enum(AXIS_SOURCE, upper=True),
            cv.Optional(CONF_Z_AXIS, default="Z"): cv.enum(AXIS_SOURCE, upper=True),
            cv.Optional(CONF_X_SIGN, default="POSITIVE"): cv.enum(AXIS_SIGN, upper=True),
            cv.Optional(CONF_Y_SIGN, default="POSITIVE"): cv.enum(AXIS_SIGN, upper=True),
            cv.Optional(CONF_Z_SIGN, default="POSITIVE"): cv.enum(AXIS_SIGN, upper=True),
        }
    ),
    _validate_axis_remap,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BNO055Component),
            cv.Optional(CONF_OPERATION_MODE, default="NDOF"): cv.enum(
                OPERATION_MODES, upper=True
            ),
            cv.Optional(CONF_USE_EXTERNAL_CRYSTAL, default=True): cv.boolean,
            # Auto-saves ACCEL/MAG/GYRO offset registers to flash the first time
            # the chip reports full calibration (sys=gyro=accel=mag=3), and
            # rewrites them at boot so the sensor doesn't start from scratch
            # every reboot (the magnetometer in particular re-drifts easily).
            cv.Optional(CONF_SAVE_CALIBRATION, default=True): cv.boolean,
            cv.Optional(CONF_RESTORE_CALIBRATION, default=True): cv.boolean,
            cv.Optional(CONF_AXIS_REMAP): AXIS_REMAP_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_operation_mode(config[CONF_OPERATION_MODE]))
    cg.add(var.set_use_external_crystal(config[CONF_USE_EXTERNAL_CRYSTAL]))
    cg.add(var.set_auto_save_calibration(config[CONF_SAVE_CALIBRATION]))
    cg.add(var.set_restore_calibration(config[CONF_RESTORE_CALIBRATION]))

    if CONF_AXIS_REMAP in config:
        remap = config[CONF_AXIS_REMAP]
        axis_map_config = (
            remap[CONF_X_AXIS] | (remap[CONF_Y_AXIS] << 2) | (remap[CONF_Z_AXIS] << 4)
        )
        axis_map_sign = (
            remap[CONF_X_SIGN] | (remap[CONF_Y_SIGN] << 1) | (remap[CONF_Z_SIGN] << 2)
        )
        cg.add(var.set_axis_remap(axis_map_config, axis_map_sign))
