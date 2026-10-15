# FP2 fork integration — 2026-10-15

Integrated locally on `codex/integrate-fp2-forks`, merging Nailik `7c82612`
(and therefore tigercraft4 `f945586`) with the original local `bf869ba` history.
The beta tooling from the preceding review was preserved. No push or flash was
performed. The author's room-specific `fp2-sala.yaml` was omitted.

## Changes beyond Nailik

- Fixed the 2026.10 `AsyncEventSource` constructor and guarded WebUI-only headers
  so both standard and WebUI configurations compile.
- Replaced HTTP access to live zones/save fields with a mutex-protected mailbox
  and owned snapshots. The main loop alone edits entities, preferences, and the
  radar. Submitting a command and publishing its pending state are atomic;
  rename now waits for actual persistence too.
- Saves require successful preference staging, durable synchronization, and
  completed radar ACK tracking. Timeout/pending no longer means success.
  Commands carry a save generation so unrelated queued writes cannot complete
  another transaction. The card checks a new completed generation and matching
  settings rather than accepting the service call or an old matching map.
- Compiled overrides are keyed by YAML identity, independent of list position.
  Optional explicit `zone_id` values preserve protocol slots; schema validation
  rejects duplicate IDs and excessive zone counts. Runtime-zone numeric storage
  remains compatible. Effective persisted geometry is reflected in the live map.
- Fixed an inherited accelerometer logging-format warning and updated the browser
  test fixtures from obsolete JSON targets to the firmware's base64 wire format.

## Selective nzzane ports

- Echo the incoming sequence in report ACKs and reverse-read replies, sent
  immediately so queued writes cannot block them. Retries retain their sequence;
  incoming ACKs match the actual in-flight register and sequence.
- Encode angle reverse-read responses as UINT16.
- Recover from lost radar traffic or a later radar boot report, cancel incomplete
  save tracking, and replay configuration. Preserve the requested operating mode,
  sleep geometry, and reporting setting. Traffic timeout defaults to 60 seconds
  and can be disabled with `radar_timeout: 0s`.
- Add opt-in target-derived presence, motion, and per-zone counts with absence
  expiry, edge/interference filtering, and separate doorway/activity filters.
  Native event handling remains the default. In derived mode, `target_count`
  counts filtered targets; the raw target stream remains available to the card.
- Share configurable wall coordinate scale between firmware and the retained
  Nailik card, and correct the wall-mode nearest-distance scale.

The replacement nzzane card, direct I²C ownership, full 20×16 editor, stock boot
replay, and default AI/activity filtering were deliberately excluded. They
conflict with the retained I²C/card/configuration approach or require device
evidence before changing detection defaults. The retained editor uses 14×14
geometry within the 20×16 protocol map.

## Verification

The package index was refreshed and resolved **ESPHome 2026.10.0b4** as the latest
2026.10 prerelease. Both the project lockfile and executed `mise` task use that
exact version. Builds use ESP-IDF 5.5.5 and compile without uploading.

Full builds passed for:

| Configuration | Coverage |
| --- | --- |
| Standard | Current example, shared I²C and OPT3001, RGB LED, runtime save actions |
| WebUI | Standard plus device-hosted editor, HTTP API and live event stream |
| Expanded features | WebUI plus derived presence/filtering, 20-unit wall scale, explicit zone ID, per-zone count, operating mode, sleep/fall entities, debug logging and telnet mirror |

Build inputs and logs are under `/tmp/fp2-integration` (`standard.yaml`,
`webui.yaml`, `features.yaml`, and their `*-build.log` files). They use dummy
credentials, so their firmware images are review artifacts, not deployment
images for your network.

`mise run test` passed four host C++ test executables, five Python schema tests,
and three Bun save-result regressions. Coverage includes malformed multi-target
records, CRC/frame encoding, mailbox admission and concurrent snapshot copying,
preference staging/sync failures, pending versus completed saves, stable zone
keys, ID validation, coordinate scaling, filtering and per-zone derived counts.
The mailbox test also passed AddressSanitizer/UndefinedBehaviorSanitizer and
ThreadSanitizer runs.

All **93 browser assertions** passed across the codec, editor, geometry, export,
and import suites. The final browser run reported zero console errors. One old
test was corrected to allow reuse of a free local protocol slot, and target
fixtures now exercise actual base64 decoding instead of generating decode errors.
Fixture warnings concern deliberately malformed/import/missing-entity data.
The lockfile and whitespace checks also passed.

## Upgrade and remaining device checks

Update the card and firmware together because save confirmation now requires
`save_status` in the map response. Keep compiled YAML zone IDs/names stable.
Old numeric compiled overrides are intentionally ignored because their identity
cannot be recovered safely after reordering; save those zones again. Runtime
overrides and names retain their existing keys.

Persisted settings represent desired configuration. A radar timeout can leave
some writes applied and others pending; a failed flush may also be retried later
by ESPHome. Failure messages therefore make no promise of rollback or unchanged
settings. Successful ACKs establish receipt, not physical detection accuracy or
read-back verification.

No actual device, Home Assistant server, power-loss cycle, or radar recovery was
exercised. Before deployment, validate wall/corner calibration, multiple people,
stationary occupants, doorway behavior, sleep/fall mode transitions, hardware
restart recovery, save failures, and persistence across power cycles. Rebuild
and retest when the final 2026.10 release becomes available.
