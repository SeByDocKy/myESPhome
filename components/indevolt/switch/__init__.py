import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import (
    CONF_INDEVOLT_ID,
    IndevoltComponent,
    apply_poll,
    g2,
    hub_generation,
    indevolt_ns,
    make_final_validate,
    with_poll,
)

DEPENDENCIES = ["indevolt"]

IndevoltSwitch = indevolt_ns.class_("IndevoltSwitch", switch.Switch, cg.Parented.template(IndevoltComponent))

# Switches are read back from the device on every poll and are not restored at boot (the class is not a
# Component), so nothing is written to the battery on startup. All of them are SolidFlex/PowerFlex 2000 only.
# (config key, points per generation, read point, write point, raw read-back value meaning "on", icon)
SWITCHES = [
    ("light", g2(7171), 7171, 7265, 1, "mdi:led-on"),
    # 2618 reports 1001 when grid charging is enabled.
    ("grid_charging", g2(2618), 2618, 1143, 1001, "mdi:transmission-tower-import"),
    ("bypass", g2(680), 680, 7266, 1, "mdi:electric-switch"),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INDEVOLT_ID): cv.use_id(IndevoltComponent),
        **{
            cv.Optional(key): with_poll(switch.switch_schema(IndevoltSwitch, icon=icon))
            for key, _p, _r, _w, _on, icon in SWITCHES
        },
    }
)


def _entries(config):
    for key, points, read, write, on_value, _icon in SWITCHES:
        if key in config:
            yield key, points, config[key], (read, write, on_value)


FINAL_VALIDATE_SCHEMA = make_final_validate(_entries)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_INDEVOLT_ID])
    for _label, _points, conf, (read, write, on_value) in _entries(config):
        s = await switch.new_switch(conf)
        await cg.register_parented(s, hub)
        cg.add(s.set_write_point(write))
        cg.add(hub.add_switch(read, write, on_value, s))
        apply_poll(hub, read, conf)
