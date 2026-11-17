# neoduo-tools

Standalone Python 3 helpers for the NEO-Duo binary `config` file (on-screen
control layout). Nothing here is inside the repo and nothing is pushed.

Verified against sevesghost/NEO-Duo `master` `9350574`
(`EmuFramework/src/vcontrols/VController.cc`, `configFile.ccm`,
`VController.hh`, `AppKeyCode.hh`, `Option.hh`, `2DOrigin.h`).

## Usage

```bash
# Inspect a real or synthetic config (flags whether toggleDuoLayout is in key 113)
python3 neocfg.py decode path/to/config

# Compare UI groups in two files (presence, both layoutPos, state).
# Use this to check a file after "delete button, quit, relaunch, save".
python3 neocfg.py diff before.cfg after.cfg

# Build a synthetic file (one variant per invocation)
python3 neocfg.py make out.cfg                         # toggle present, SHOWN, default pos
python3 neocfg.py make out.cfg --no-duo-toggle         # toggle removed
python3 neocfg.py make out.cfg --no-113                # old file, no key 113
python3 neocfg.py make out.cfg --v1-only               # only historic keys 68/100/101
python3 neocfg.py make out.cfg --duo-toggle-moved --duo-toggle-state 2
python3 neocfg.py make out.cfg --duo-toggle-moved --duo-toggle-state 0 \
        --pos0 80,120,CB --pos1 -40,250,LT

# Round-trip every variant and refresh samples/
python3 neocfg.py selftest --samples-dir samples
```

`--duo-toggle-state` is a `VControllerState`: `0=OFF`, `1=SHOWN`, `2=HIDDEN`.
`--duo-toggle-moved` uses custom landscape/portrait positions (defaults below)
and, if state is omitted, sets state to `HIDDEN`.

`--pos0` / `--pos1` are `x,y,ORIGIN` with ORIGIN one of
`CT RT RC RB CB LB LC LT C` (the `_2DOrigin` cartesian shortcuts).

## Sample files

Written to `samples/`. These are the four main test cases plus the
moved/hidden variant needed once the fix starts saving position and state.

| File | Test case | What is in the file |
|---|---|---|
| `samples/toggle-present.cfg` | Toggle present; after "add it back" or a default save | Keys `125` + `113`. `113` has Open Menu, Fast-forward/Rewind, and `toggleDuoLayout` at state `SHOWN`, both layouts `(0, 300)` origin `CT2DO` (what `syncDuoModeButton` + `setDuoPosition(..., 0.f, 30.f)` writes). |
| `samples/toggle-removed.cfg` | Delete the ☰ button, then save | Keys `125` + `113`. `113` has only Open Menu and Fast-forward/Rewind. First group's `pos.x` is **not** `-1`, so `configure()` will not call `resetUIGroups()` and resurrect the toggle. |
| `samples/no-113.cfg` | Old file that never had `CFGKEY_VCONTROLLER_UI_BUTTONS_V2` | Key `125` only (default two-player Joint header). Loading this leaves the constructor sentinel on `uiElements[0]`, so `configure()` resets UI groups and the toggle appears by default. |
| `samples/v1-only.cfg` | Pre-V2 file (keys `68`, `100`, `101` only) | Same expected load behaviour as `no-113.cfg` (no 112/113/125). **Payloads are placeholders** — see Uncertainties. |
| `samples/toggle-moved-hidden.cfg` | Move and hide the toggle, then save (both orientations) | Same groups as `toggle-present.cfg`, but `toggleDuoLayout` is `HIDDEN` (2), landscape `(80, 120)` origin `CB2DO`, portrait `(-40, 250)` origin `LT2DO`. |

On-screen positions for Open Menu `(-16, 24)` and Fast-forward/Rewind `(24, 24)`
are **synthetic pixels**, chosen only so `pos.x != -1`. A real device writes
window-dependent pixel offsets from `resetUIPositions()`.

Gamepad elements in key `125` are omitted (`elements=0`). The V3 *header*
still matches `writeDuoButtonsConfig` (flags, sizes, alphas). These files exist
to exercise UI key `113`.

## Format (current C++ writer)

Little-endian, as `FileIO::put` writes native memory on Android/ARM and x86_64.

```
uint8  header = 2                 # block size field width
block:
  uint16 size                     # includes the 2-byte key
  uint16 key
  uint8  payload[size - 2]
```

Relevant keys: `112` device buttons V2, `113` UI buttons V2, `125` device buttons V3.

**113** (`writeUIButtonsConfig` / `configDataSizeBytes(..., true)`):

```
int8   configId                   # reserved, writer stores 0
uint8  elementCount
N × UI element (writeToConfig without duoExtra):
  int8   type                     # 0 = UI button group
  int8   rowItems
  uint8  origin.pack()            # x in low nibble, y in high nibble; PackedType is uint8
  uint8  keyCount                 # writeSizedData<uint8_t>
  N × 4  KeyInfo
  int16  layoutPos[0].pos.x       # landscape, S2Pt
  int16  layoutPos[0].pos.y
  uint8  layoutPos[0].origin.pack()
  int16  layoutPos[1].pos.x       # portrait
  int16  layoutPos[1].pos.y
  uint8  layoutPos[1].origin.pack()
  uint8  VControllerState         # 0 OFF, 1 SHOWN, 2 HIDDEN
```

Default Android UI (from `defaultUIGroups` + `syncDuoModeButton`):

- Open Menu (`AppKeyCode::openMenu = 1`): `rowItems = 4` (the
  `VControllerUIButtonGroup(span, origin)` constructor, not the default of 1),
  origin `RT2DO`.
- Fast-forward/Rewind (`toggleFastForward = 9`, `rewind = 15`): `rowItems = 4`,
  origin `LT2DO`. Present because `Config::Input::TOUCH_DEVICES` is true on
  Android.
- Mode / ☰ (`toggleDuoLayout = 20`): `rowItems = 1`, origin `CT2DO`,
  positions `(0, 300)` both layouts. `sizeMM100x` / `duoFlags` are **not**
  written today (`writeToConfig(e, io)` is called without `duoExtra`).

**KeyInfo** is 4 bytes: three `uint8` codes (`ZArray<KeyCode,3>`) plus a
`KeyFlags` bitfield byte. An app key is `code, 0, 0, 0x01`. Flag bits (GCC /
Clang Itanium ABI, low bit first): `appCode`, `turbo`, `toggle`, `bothPlayers`,
`deviceId:4`.

**125** (`writeDuoButtonsConfig`): the current writer always sets flags bit 8,
so `sharedSize` (int16), `sharedAlpha` (uint8) and `lockPlayerSizes` (uint8)
are always present, then a count and, if `twoPlayer_`, each device element
plus a 5-byte duo extra (`int16 sizeMM100x`, `uint8 ownAlpha`, `uint8 duoFlags`,
`uint8 duoRole`). Defaults: `twoPlayer_=true`, `duoLayout_=Joint` (configId 1),
flags `0x09`, sizes `1000`, alphas `127/127/102`. Key `112` is only written
when `twoPlayer_` is false.

**112** is the older device list: `int8 emuDeviceId`, `int8 configId`,
`uint8 count`, then elements without duo extras.

## Decoder tolerance (upcoming fix)

`writeUIButtonsConfig` may later save `duoFlags` / `sizeMM100x` for UI buttons,
either as trailing bytes on key `113` or as 5 bytes after each element, or
under a brand-new key. `decode`:

- prints unknown keys with a hex dump of the payload (does not crash);
- if a 113 (or 112/125) block has leftover bytes, prints them as hex;
- if the leftover is exactly `5 * elementCount` at the end of 113, also
  interprets it as per-element duo extras;
- if a first pass fails, retries with interleaved per-element duo extras.

## Uncertainties

1. **v1 keys 68 / 100 / 101.** The current tree does not read or write them,
   and this checkout has a single git commit, so the historic payload layout
   is not here. `make --v1-only` writes a valid *container* (header + size/key
   blocks) with only those keys. Payloads are placeholders (eight `int16` `-1`
   for key 68, a single `0x00` for 100 and 101). That is enough for the
   "old file, no 113, default toggle should appear" case, but it is **not**
   a byte-for-byte copy of a file a past C++ writer would have produced.

2. **Open Menu / Fast-forward positions.** Pixel offsets depend on the window
   and button size (`resetUIPositions`). Samples use fixed non-sentinel values.
   Only the toggle's default `(0, 300)` `CT2DO` is taken verbatim from
   `setDuoPosition`.

3. **Empty key 125 element list.** A real save after `buildDuoGamepad` would
   contain many device buttons. The V3 header bytes are exact; the element list
   is left empty on purpose.

4. **`KeyFlags` bit order** is assumed to be GCC/Clang low-bit-first, which
   matches the Android NDK and Linux builds of this project. `sizeof(bool)` is
   assumed 1, which is how `visualizeBounds` / `showBoundingArea` are written.

5. **`TOUCH_DEVICES`** is true on Android/iOS and false on Linux/X11. Samples
   follow the Android layout (they include the left Fast-forward/Rewind group).
