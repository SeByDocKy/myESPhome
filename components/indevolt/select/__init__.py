import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from .. import (
    CONF_INDEVOLT_ID,
    IndevoltComponent,
    apply_poll,
    both,
    g2,
    hub_generation,
    indevolt_ns,
    make_final_validate,
    with_poll,
)

DEPENDENCIES = ["indevolt"]

IndevoltSelect = indevolt_ns.class_("IndevoltSelect", select.Select, cg.Parented.template(IndevoltComponent))

# (config key, availability per generation, read point or 0, write point, read offset, icon, [(option, raw value), ...])
SELECTS = [
    # Work mode, read back from point 7101, written to 47005. SolidFlex/PowerFlex 2000 only: the BK1600 is
    # only ever driven in Real-Time Control (the official integration offers no mode choice for it).
    (
        "work_mode",
        g2(0),
        7101,
        47005,
        0,
        "mdi:cog",
        [("Self-Consumed Prioritized", 1), ("Real-Time Control", 4), ("Charge/Discharge Schedule", 5)],
    ),
    # State of the running real-time command (raw write of the first value of point 47015). Read back from
    # point 6001 (1000 + state). Prefer the `power_setpoint` number or the outputs: they send the complete command.
    (
        "realtime_state",
        both(0),
        6001,
        47015,
        1000,
        "mdi:swap-vertical",
        [("Standby", 0), ("Charging", 1), ("Discharging", 2)],
    ),
    # Load setting (SolidFlex/PowerFlex 2000). Write-only: the official integration cannot read it back.
    (
        "load_setting",
        g2(0),
        0,
        1,
        0,
        "mdi:power-plug",
        [("Smart Plug", 1), ("Meter", 2), ("Key Load", 3), ("Custom", 4)],
    ),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INDEVOLT_ID): cv.use_id(IndevoltComponent),
        **{
            cv.Optional(key): with_poll(select.select_schema(IndevoltSelect, icon=icon))
            for key, _avail, _read, _write, _offset, icon, _options in SELECTS
        },
    }
)


def _entries(config):
    for key, availability, read, write, offset, _icon, options in SELECTS:
        if key in config:
            yield key, availability, config[key], (read, write, offset, options)


FINAL_VALIDATE_SCHEMA = make_final_validate(_entries)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_INDEVOLT_ID])
    for _label, _availability, conf, (read, write, offset, options) in _entries(config):
        s = await select.new_select(conf, options=[name for name, _raw in options])
        await cg.register_parented(s, hub)
        values = [raw for _name, raw in options]
        cg.add(s.set_write_point(write))
        cg.add(s.set_values(values))
        cg.add(hub.add_select(read, write, offset, values, s))
        apply_poll(hub, read, conf)
