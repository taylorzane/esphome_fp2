# FP2 fork review — 2026-10-15

This is the original pre-integration review. The later implementation and
verification are documented in [INTEGRATION.md](INTEGRATION.md).

## Recommendation

Use the tigercraft4/Nailik line as the basis for general-purpose improvements.
Nailik is a strict descendant of tigercraft4, adding only three card commits;
there is no reason to merge both independently. Port selected nzzane protocol
and firmware-99 improvements rather than merging its entire alternative stack.
Resolve the findings below before relying on runtime saves or enabling the
optional device-hosted editor.

No fork changes were merged. Local changes only establish the ESPHome
2026.10 beta toolchain and document this review. Nothing was pushed or flashed.

## Exact histories inspected

Counts describe commits on each side of the merge base, not equivalent patches.

| Snapshot | Commit | Local-only / remote-only commits |
| --- | --- | --- |
| Local main | `bf869ba` | — |
| Your origin/main | `10dc683` | 2 / 54 |
| Jaano upstream/main | `7a6ade4` | 0 / 2 |
| tigercraft4/main | `f945586` | 2 / 171 |
| Nailik/main | `7c82612` | 2 / 174 |
| nzzane/main | `50335c1` | 2 / 12 |

Your GitHub fork already contains the first 54 commits of the tigercraft4 line
relative to its shared ancestor with this checkout. It is 117 commits behind
tigercraft4 and 120 behind Nailik, with no unique commits relative to those two.
The local-only commits are the I2C/logging update `47e2cd6` and illuminance
example `bf869ba`; preserve their intent when resolving conflicts.

Read-only three-way merge inspection produced conflict markers when combining
the current local main with origin or any of the three requested forks,
including the radar component. These are integration changes, not fast-forwards.

## What each fork contributes

### tigercraft4

Compared with the local common ancestor, this is a substantial extension:

- Shared ESPHome I2C accelerometer support and RGB LED configuration, radar
  initialization fixes, report-handler fixes, and broader diagnostic entities.
- A Home Assistant zone editor: painting, YAML import/export, runtime saves,
  zone creation/deletion/renaming, and persistent ESP32 preferences.
- An optional device-hosted editor with live target streaming; separate lite card.
- Operating-mode selection, sleep heartbeat/setup, fall reports, per-zone people
  counts, debug frame logging, and an optional default-disabled telnet mirror.
- Recent target decoding fixes, count-change publication, and disabled-by-default
  AI filtering. The latest parser commit includes a host-side regression test.

Much of the earlier work, including initial zone editing and persistence, is
already in your origin/main. The newer device web UI and runtime zone lifecycle
are the largest additions beyond your fork. Worth integrating in stages; retain
the shared ESPHome I2C design and omit the author's room-specific `fp2-sala.yaml`.

Useful starting points: [latest target parser fix](https://github.com/tigercraft4/esphome_fp2/commit/f945586f7920263694ee2d3d9eb9a96e34789dae),
`5d03577` (optional WebUI dependency), `36bc7a4` (runtime entity API fix), and
`147ee1f` (Home Assistant card action compatibility). These have dependencies;
the list is not an independently cherry-pickable sequence from the old local main.

### Nailik

Exactly tigercraft4 plus `7d54131`, `29abfd3`, and `7c82612`, all touching only
`card.js`. The aggregate changes center wall-mode targets above the grid, expose
`wall_units_per_cell` (default 50; 20 can be supplied for firmware-99 scaling),
and preserve target ID zero with `??` instead of `||`.

**Worth taking together with the tigercraft4 card.** The wall transform replaces
an explicitly unverified placeholder. Confirm scale and direction on the actual
radar firmware; the default is not a universal physical calibration. The ID-zero
fix is independently useful.

[Exact card delta](https://github.com/Nailik/esphome_fp2/compare/f945586f7920263694ee2d3d9eb9a96e34789dae...7c82612206d1f491444d3df6dffd4acbee8bf9d3).

### nzzane

Twelve commits form a separate firmware-99-oriented implementation:

- BOOL location-stream enable, echoed reply sequence numbers, separate in-flight
  writes, stock-like initialization, and heartbeat-based radar recovery.
- Presence/motion/counts derived from targets when native events are missing;
  exclusion/interference/doorway filtering and per-zone derived motion.
- Live settings selects/switches, persistent maps and zones, experimental sleep
  entities, and a replacement editor supporting the full 20×16 protocol map.
- Wall-mode defaults and scaling, viewport dimensions/cropping, and action-prefix
  discovery from the device name.
- Direct ESP-IDF I2C ownership with built-in OPT3001 handling and a FreeRTOS task.

**Worth selectively porting**, particularly sequence-number handling, radar
recovery, full-grid support, and opt-in derived presence for firmware 99.
Do not combine its I2C driver with an ESPHome `i2c:`/`opt3001` configuration on
the same physical bus. Its own example removes that shared-bus configuration.
Its schema, actions, persistence, and card differ from the tigercraft4 line;
replacing the firmware while retaining the other card is not a supported mix.

The large [protocol commit](https://github.com/nzzane/esphome_fp2/commit/5356fd9e6a4abc87c8bd6376572b9fcdf2fa51bf)
bundles many behaviors. Prefer extracting small changes over cherry-picking it
as a single fix. Its default AI filter is enabled and its active-target filter
is enabled, unlike the latest tigercraft4 approach. Validate those choices with
captures from the actual radar before changing detection behavior.

## Findings affecting integration

0. **Optional WebUI does not compile on ESPHome 2026.10.0b4 as published.**
   [The event-source constructor](https://github.com/tigercraft4/esphome_fp2/blob/f945586f7920263694ee2d3d9eb9a96e34789dae/components/aqara_fp2/fp2_component.cpp#L228)
   passes a string literal where the beta API now requires `StringRef`.
   The full build with both WebUI IDs enabled failed at that exact call;
   ordinary example builds omit this optional code and pass. Use
   `esphome::StringRef::from_lit("/zones/events")` as the first argument.
   A separate disposable build passed with that adjustment; it does not resolve the
   runtime concurrency concerns below.

1. **Optional WebUI has cross-task data races (tigercraft4 and Nailik).**
   [The HTTP handlers](https://github.com/tigercraft4/esphome_fp2/blob/f945586f7920263694ee2d3d9eb9a96e34789dae/components/aqara_fp2/zones_web_handler.h#L114-L139)
   read zones and save status while the main loop mutates them.
   [Reserving the vector](https://github.com/tigercraft4/esphome_fp2/blob/f945586f7920263694ee2d3d9eb9a96e34789dae/components/aqara_fp2/fp2_component.h#L795-L804)
   prevents reallocation but does not synchronize vector size, zone fields, or
   [plain save flags and the error string](https://github.com/tigercraft4/esphome_fp2/blob/f945586f7920263694ee2d3d9eb9a96e34789dae/components/aqara_fp2/fp2_component.h#L900-L925).
   Concurrent editing/status reads can observe inconsistent state and constitute
   C++ data races. Serve snapshots prepared on the main loop or protect shared
   state appropriately. Keep the optional device editor disabled pending this.

2. **Save success can be reported without completion or durable persistence.**
   In the shared [example actions](https://github.com/tigercraft4/esphome_fp2/blob/f945586f7920263694ee2d3d9eb9a96e34789dae/example_config.yaml#L73-L112),
   expiration of `wait_until` proceeds to `api.respond`, whose success checks
   `save_ok()` but not `!save_pending()`. Queued work can outlast that wait while
   the failure flag remains clear. Also, [preference writes](https://github.com/tigercraft4/esphome_fp2/blob/f945586f7920263694ee2d3d9eb9a96e34789dae/components/aqara_fp2/fp2_component.cpp#L465-L475)
   ignore both `save()` and `sync()` results. Require completed ACK tracking and
   successful persistence before claiming a save succeeded; return a distinct
   timeout/error otherwise.

3. **Nzzane can delay radar replies behind blocked writes.**
   Its [queue loop](https://github.com/nzzane/esphome_fp2/blob/50335c15e17f8606a9b60e48889883c2040c78e6/components/aqara_fp2/fp2_component.cpp#L462-L500)
   returns when the front command is a WRITE and another write is awaiting ACK.
   Report ACKs and reverse-read replies are appended to the same queue, so they
   cannot bypass that front write. During initialization or a slow/failed write,
   replies can wait through retries and other queued writes. Preserve the
   tigercraft4 immediate-reply approach when porting nzzane's sequence echoing.

4. **Persistent zone overrides follow list position across reflashes.**
   The tigercraft4 line keys [saved overrides by numeric zone ID](https://github.com/tigercraft4/esphome_fp2/blob/f945586f7920263694ee2d3d9eb9a96e34789dae/components/aqara_fp2/fp2_component.cpp#L447-L462),
   while compiled IDs follow YAML ordering. Reordering compiled zones can load
   another zone's saved geometry. Introduce stable IDs or an explicit override
   migration/reset workflow before depending on this across configuration edits.

## Suggested integration order

1. Reconcile local main with your already-expanded origin/main on an integration
   branch, preserving the local I2C/illuminance changes and the beta tooling.
2. Integrate Nailik once, bringing tigercraft4's history with it. Review the large
   runtime-zone additions and address the save findings; leave the device WebUI
   disabled until cross-task state access is fixed.
3. Port nzzane's sequence echoing and radar recovery independently. Add firmware
   profiles or explicit options before porting its initialization/detection
   defaults, full-grid geometry, and derived presence.
4. Test the final combined build on the device: multiple people, wall/corner
   geometry, stationary occupants, doorways, save timeouts, power-cycle durability,
   radar restart recovery, and Home Assistant actions/entity discovery.

## Toolchain and verification

The target is **ESPHome 2026.10.0b4**, the latest beta resolved from the package
index during this review. `pyproject.toml` and `uv.lock` pin it; Python 3.12 is
configured in `.python-version` and `mise.toml`. `mise run esphome -- version`
was executed and returned `2026.10.0b4`. Upgrade the exact pin and regenerate the
lockfile when stable ships, then rebuild; beta compatibility is not proof about
the future final release.

Checks used disposable source snapshots with dummy secrets under
`/tmp/fp2-fork-review`. Build logs remain there. These binaries are review
artifacts, not configured for your network and not intended for flashing.

- Earlier configuration/source-generation checks passed with ESPHome 2026.9.1;
  beta full-build results below supersede them for the requested target.
- The existing host C++ target-parser regression test passed.
- Bun parsed/bundled Nailik and nzzane cards successfully. Geometry smoke checks
  passed for corner transformation, double mirroring, and Nailik's wall scaling
  at both 50 and 20 units per cell. These were not browser/Home Assistant tests.
- `uv lock --check` and `git diff --check` passed.
- No device execution, flashing, physical calibration, Home Assistant session,
  power-loss test, or final combined-fork build was performed.

| Full build, ESPHome 2026.10.0b4 / ESP-IDF 5.5.5 | Result | Static RAM | Application image |
| --- | --- | --- | --- |
| Local main, unchanged firmware source | Passed | 43,144 B | 900,413 B |
| origin/main | Passed | 45,268 B | 933,641 B |
| Jaano upstream/main | Passed | 43,444 B | 907,493 B |
| Nailik/main, standard example | Passed | 45,428 B | 936,709 B |
| tigercraft4/main | Same firmware and example as Nailik; covered by its build | — | — |
| nzzane/main | Passed | 44,876 B | 939,277 B |
| Nailik with optional device WebUI enabled | Failed: event-source `StringRef` API | — | — |
| Same WebUI configuration with the one-line `StringRef::from_lit` adjustment | Passed (disposable copy only) | 45,684 B | 1,057,861 B |

The standard examples do not enable every optional entity or operating mode.
Static memory figures do not measure runtime heap use or fragmentation. None
of these results establishes device behavior or future stable-release support.

## Remote configuration

| Remote | Repository |
| --- | --- |
| origin | `git@github.com:taylorzane/esphome_fp2.git` |
| upstream | `git@github.com:Jaano/esphome_fp2.git` (preserved previous origin) |
| tigercraft4 | `https://github.com/tigercraft4/esphome_fp2.git` |
| Nailik | `https://github.com/Nailik/esphome_fp2.git` |
| nzzane | `https://github.com/nzzane/esphome_fp2.git` |

All remotes were fetched. Local main tracks origin/main, and `remote.pushDefault`
is origin. Main is still ahead by 2 and behind by 54 commits; no reset/rebase/merge
was performed.
