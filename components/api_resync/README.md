# api_resync

ESPHome external component that forces Home Assistant to refresh entity metadata that the native API only sends once, at connection time: `number` `min_value` / `max_value` / `step`, units, options, etc.

## Why

The native API sends entity descriptions (`ListEntities*Response`) only when a client connects. There is no message for the device to say "my entity changed", so changing the range of a `number` at runtime is invisible to Home Assistant until it reconnects.

`api_resync` makes that reconnection happen on demand, without rebooting the node and without touching the WiFi link:

1. The device sends a `DisconnectRequest` to every connected API client.
2. `aioesphomeapi` answers with a `DisconnectResponse` and flags the disconnect as expected.
3. Home Assistant reconnects by itself and lists the entities again.
4. Existing entities are updated in place (same `entity_id`, same history).

Every resync briefly marks all entities of the device as `unavailable`, so requests are debounced and rate limited.

## Warning

This component relies on ESPHome internals (`api::global_api_server->active_clients()`, `APIConnection::send_message()` and `APIConnection::on_fatal_error()` being public). It was written against the ESPHome `dev` branch and may not compile on older releases.

## Installation

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [api_resync]

api:

api_resync:
  min_interval: 5s
  delay: 500ms
```

## Configuration

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `id` | id | auto | Component id, only needed with several instances |
| `delay` | time | `500ms` | Debounce: requests within this window collapse into one resync |
| `min_interval` | time | `5s` | Minimum time between two resyncs. An early request is postponed, never lost |

## Actions

### `api_resync.resync`

Schedules a resync.

```yaml
on_value:
  then:
    - api_resync.resync:
```

### `api_resync.set_number_range`

Changes `min_value`, `max_value` and optionally `step` of a number, optionally clamps its current state into the new range, then schedules a resync. Does nothing if the range is unchanged.

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `number_id` | id | required | The `number` to update |
| `min_value` | templatable float | required | New minimum |
| `max_value` | templatable float | required | New maximum |
| `step` | templatable float | unchanged | New step |
| `clamp_state` | boolean | `true` | Publish the current state clamped into the new range |

```yaml
select:
  - platform: template
    name: ${name}_phases
    id: phases
    options:
      - 1 phase
      - 3 phases
    optimistic: true
    restore_value: true
    on_value:
      then:
        - api_resync.set_number_range:
            number_id: power_limit
            min_value: !lambda |-
              return (i == 0) ? 1380.0f : 4140.0f;
            max_value: !lambda |-
              return (i == 0) ? 3680.0f : 11040.0f;
```

### `api_resync.set_select_options`

Replaces the option list of a `select`, keeps the current selection when its text is still in the new list, then schedules a resync. Does nothing if the option list is unchanged.

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `select_id` | id | required | The `select` to update |
| `options` | list of strings or lambda | required | New options. A lambda must return a `std::vector<std::string>` |
| `fallback_index` | int | `0` | Option to select when the current one is no longer available (falls back to `0` if out of range) |

```yaml
select:
  - platform: template
    name: ${name}_mode
    id: mode
    options:
      - eco
      - fast
    initial_option: eco
    optimistic: true

  - platform: template
    name: ${name}_phases
    id: phases
    options:
      - 1 phase
      - 3 phases
    optimistic: true
    restore_value: true
    on_value:
      then:
        - api_resync.set_select_options:
            select_id: mode
            options: !lambda |-
              if (i == 0) return {"eco", "fast"};
              return {"eco", "fast", "turbo"};
```

How the selection is handled: a select state is only an index, so the component remembers the current option by text, swaps the list, then looks the text up in the new list. If it is found at another position, the state is republished with the new index; if it is gone, `fallback_index` is published. Either way the select's `on_value` fires, because `publish_state()` always calls the state callbacks. Empty lists are rejected.

## Notes

- Option strings are owned by the action: `SelectTraits` only stores `const char *` pointers.
- Changes made at runtime are not persistent. After a reboot, `number` ranges and `select` options come back as defined in the YAML, so re-apply them at boot (for example from the `on_value` of the entity that drives them; `restore_value` triggers it at boot).
- With `restore_value` on a template select, the saved value is an index into the list that is active at boot, i.e. the YAML list.
- If no client is connected when the resync fires, nothing is sent: Home Assistant lists entities when it connects anyway.
- `restore_value` triggers `on_value` at boot, before Home Assistant connects, so the first connection already sees the right range.
- All API clients are disconnected, not only Home Assistant.

## License

MIT License

Copyright (c) 2026 e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
