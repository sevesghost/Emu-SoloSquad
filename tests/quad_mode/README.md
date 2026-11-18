# Saturn-Quad 4-player touch layout — host tests (west)

    tests/quad_mode/run.sh [filter]                 # default: pr4logic backend, real QuadControls.hh from this repo
    QUAD_BACKEND=real tests/quad_mode/run.sh        # full VController backend (all PENDING until backend_real_impl.inc exists)
    QUAD_SRC=/path/to/Saturn-Quad tests/quad_mode/run.sh
    QUAD_BACKEND=model tests/quad_mode/run.sh       # spec self-check of the cases (labelled MODEL)
    QUAD_BACKEND=model QUAD_MODEL_BUG=resurrect_mode run.sh   # injected bug: Mode cases must FAIL
    QUAD_STRICT=1 ...                               # exit 3 if anything is PENDING

g++ >= 13, `-std=c++23 -Wall -Werror`. run.sh compiles `core/main.cc`, `core/backend_<backend>.cc`
and **every `cases/*.cc`** (glob). Exit 1 on any FAIL. XFAIL/XPASS never change the exit code.

## Writing cases (staff)
One file per owner: `cases/p1_*.cc` .. `cases/p4_*.cc` (per set), `cases/cross_*.cc` (isolation,
mode switch, old config). Include only `"quad_harness.hh"`. Do not edit `core/` (west owns it).

```cpp
#include "quad_harness.hh"
using namespace quad;
CASE("p2 C delete Mode survives 2 relaunches") {
	freshInstall();                 // real default buttons (A2/G2 style), NOT Mode-only
	removeBtn(uiSet, Mode);
	closeLayoutEditor();            // editor close must save
	relaunch(2);
	CHECK(!exists(uiSet, Mode));
	CHECK(savedDuoFlags(save(), uiSet, OpenMenu) & removedFlag); // bit 7 from the saved BYTES
}
XFAIL_CASE("p2 I delete all UI, add back non-Mode one by one") { ... } // known issue -> XFAIL/XPASS
CASE("p2 B color") { if(...not decidable...) PENDING("why"); ... }
```
Names: prefix with the owner (`p1 `..`p4 `, `cross `) then the NEO-Duo case letter (A..I, A2, G2).

## Helper API (core/quad_harness.hh)
Sets: `0..3` (= duoRole = Saturn port 1..4), `uiSet` (-1) = shared UI buttons. `FOR_SETS(s)` loops 0..3.
Keys: game `Dpad A B C X Y Z L R Start` (`gameKeys()`), UI `OpenMenu FastForward Rewind Mode`
(`defaultUIKeys()`; Mode = three-line Toggle Control Layout / `toggleDuoLayout`).

| helper | meaning |
|---|---|
| `freshInstall()` | fresh install, 4 sets, real default UI groups (Open Menu, FF/Rewind) + Mode added by sync |
| `freshInstallModeOnlyUI()` | legacy NEO-Duo A..H start (UI = Mode only) |
| `save()` → bytes | writes config (key 113 UI_BUTTONS_V2 + trailer), returns the file bytes |
| `relaunch()` / `relaunch(n)` | save → new instance → load → configure (n times) |
| `loadConfig(bytes)` | new instance from given bytes (e.g. `neoDuo2PlayerConfig()`) |
| `openLayoutEditor()` / `closeLayoutEditor()` | editor close must persist (onLayoutEditorClosed) |
| `killAfterEditor()` | process killed right after closing editor; reload what was written |
| `exists(set,key)` `hidden(set,key)` `pos(set,key)` `count(set)` | queries |
| `removeBtn` `removeAllUI` `addBack` `moveBtn(set,key,Pos)` `hideBtn(set,key,bool)` | edits |
| `press(set,key)` → `PortInputs` | touch a button; `[i]` = bits seen on Saturn port i+1 |
| `color(set)` → `Rgb` | pad tint of a set |
| `switchLayout()` | Mode action (Joint/Separate) |
| `setPlayers(n)` / `players()` | 1/2/4-player touch layout |
| `savedDuoFlags(cfg,set,key)` / `savedDuoRole(...)` | read trailer bytes of saved key 113 |
| `neoDuo2PlayerConfig()` | NEO-Duo v1.1 2-player config (roles 0..1) |

Macros: `CASE`, `XFAIL_CASE`, `CHECK`, `CHECK_MSG(cond, std::string)`, `PENDING(why)`, `SKIP(why)`.

## Backends
- **real**: `extract.py` probes the tree for the 4P symbols below; if any is missing every case is
  PENDING with the list. When the PR lands west writes `core/backend_real_impl.inc` (real code
  extracted from the tree, NEO-Duo method: real `configure/remove/writeUIButtonsConfig/readConfig/
  syncDuoModeButton/...`, stubs mirror real header access, `-Werror`).
- **model**: reference model of the spec, only to prove the cases are right (and that they FAIL on
  injected bugs: `QUAD_MODEL_BUG=resurrect_mode|cross_route|lose_move|lose_hide|drop_bit7|old_cfg`).

## Expected implementation names (for เหนือ)
`VController::syncDuoModeButton()`, `noteUIButtonRemoved()`, `noteUIButtonAdded()`,
`onLayoutEditorClosed()` (called from TouchConfigView/PlaceVControlsView `onDismiss`, saves),
`setDuoLayout()`, `setPlayerControls(int players)` (1/2/4), `static constexpr int maxDuoPlayers = 4`
(VController.hh), `duoRoleColor(int role)`, `duoRolePort(int role)` (role → port 0..3),
`uiButtonDuoExtraBytes` (=5: sizeMM100x i16, ownAlpha u8, duoFlags u8 [bit7 removed], duoRole u8 0..3).
Old NEO-Duo configs (roles 0..1) must load; roles 2..3 get defaults.

ROM/BIOS: none needed; never copy anything from `/workspace/west-testroms`.

## Status log
- Baseline 2P (NEO-Duo f81076b, tests/duo_mode_button, real code): A–H, A2, G2 PASS; I XFAIL
  (fix pieces exercised: syncDuoModeButton/ensureDuoModeButton, duoFlags bit7, noteUIButtonRemoved/Added,
  onLayoutEditorClosed wired from TouchConfigView/PlaceVControlsView onDismiss, saveConfigFile public).
- Quad real backend: all PENDING (no 4P code in any tree yet). Model: 48 PASS + I XPASS; every injected
  bug (resurrect_mode, cross_route, lose_move, lose_hide, drop_bit7, old_cfg) makes cases FAIL.
- Saturn core: Saturn.emu/src/main/input.cc:48 `assume(player < AppMeta::maxPlayers)` (input routed by
  `a.flags.deviceId` -> `inputBuff[player]`), input.cc:106-107 `SMPC_SetMultitap(0/1, config.multitaps[..])`,
  input.cc:108-110 `mdfnGameInfo.SetInput(idx, ...)` per `config.devs`; options.cc:81/111
  `CFGKEY_INPUT_PORT_CONFIG`; options.cc:207-209 sport1/sport2 multitap handled in onSessionOptionsLoaded.
  => ports 3-4 need a multitap enabled (or devs beyond 2 via multitap) for 4 players.

## Added 21:22
Helpers: `resetUIGroups()`, `multitapEnabled(port)`, `setPadSize/padSize`, `setPadAlpha/padAlpha`,
`pressTurbo(set,key)`, `playerSetShift/playerSetMask` (duoFlags bits 3-5). Model decoder reads a full config
file (0x02 + blocks {u16 size incl. key, u16 key, payload}); real NEO-Duo key-113 payloads → SKIP on model.
Model saves a per-set "user removed all" marker (key 0 @ -2,-2, bit7 + playerSet) so an emptied set is not
refilled, while sets absent from an old 2P file get defaults. cross 4b uses `tools/neoduo-samples/samples-pr2b`
(QUAD_NEO_SAMPLES). Case I is a normal CASE in west_core (north fixes it this round).
South targets: S1 multitap (input.cc:106-110), S2 size/alpha P3/P4 vs P2 (VController.hh:697,701,756-760;
VControllerDuo.cc:116-162), S3 turbo (EmuInput.cc:226-245), S4 playerSet bits 3-5 vs bit7, S5 setPlayerControls
must not clear bit7 (VControllerDuo.cc:64-70), S6 remove-all-UI + sync (VControllerDuo.cc:279), C4 per-set marker,
O1 save→load→save byte-identical. Bugs injectable: no_set_marker size_clobber turbo_2p no_multitap mode_switch_clears_bit7.
