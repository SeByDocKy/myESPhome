# output_combined

A native ESPHome `output` platform that fans out a single float state to
any number of other `FloatOutput` instances.

It is the compile-time-validated equivalent of a `template` output whose
`write_action` calls `output.set_level` on several outputs:

```yaml
# Equivalent template-based approach
output:
  - platform: template
    id: output_combined
    type: float
    write_action:
      - output.set_level:
          id: hms1_power_limit_percent_output
          level: !lambda return state;
      - output.set_level:
          id: hms2_power_limit_percent_output
          level: !lambda return state;
```

## Why use this instead of `template` + `write_action`

- The `outputs` list is validated by ESPHome's Python config step
  (`cv.use_id(output.FloatOutput)`), so a typo or wrong type is caught at
  compile time instead of silently doing nothing.
- No lambda/action layer at runtime — just a simple loop over a
  `std::vector`.
- Scales to any number of outputs without duplicating YAML blocks.

## File layout

`output_combined` is a **platform-only** component: it does not define a
new top-level YAML key, only a new `platform:` under the existing
`output:` domain. ESPHome requires the config schema / codegen for a
domain platform to live in a file named after that domain — hence
`output.py`, not `__init__.py`:

```
components/output_combined/
├── __init__.py          # CODEOWNERS only
├── output.py             # CONFIG_SCHEMA + to_code (the "output" platform)
├── output_combined.h
└── output_combined.cpp
```

## Installation

Copy the `components/output_combined` folder into your ESPHome
`external_components` path (local or a GitHub repo), then reference it:

```yaml
external_components:
  - source:
      type: local
      path: components
```

Or straight from GitHub:

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [output_combined]
    refresh: 10s
```

## Usage

```yaml
output:
  - platform: output_combined
    id: output_combined
    outputs:
      - hms1_power_limit_percent_output
      - hms2_power_limit_percent_output
      # - hms3_power_limit_percent_output
```

- `id` (**Required**, ID): The id of this combined output.
- `outputs` (**Required**, list of IDs): The list of `FloatOutput`
  entities to drive with the same level. At least one is required;
  there is no upper limit.
- All other options from the standard `FLOAT_OUTPUT_SCHEMA` are
  supported (`min_power`, `max_power`, `zero_means_zero`,
  `power_supply`, `inverted`).

## Behavior notes

- `OutputCombined::write_state()` calls `set_level()` (not
  `write_state()` directly) on each sub-output, so each sub-output's
  own `min_power`/`max_power`/`zero_means_zero`/`power_supply` settings
  still apply individually. This matches what the `template` +
  `output.set_level` action does.
- If `output_combined` itself has `min_power`/`max_power` configured,
  those are applied once on the combined value before it is forwarded,
  and then each sub-output applies its own clamping again on top of
  that.
