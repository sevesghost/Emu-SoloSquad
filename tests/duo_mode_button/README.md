# Duo Mode button host tests

    tests/duo_mode_button/run.sh            # build + run all, exit 1 on any FAIL
    tests/duo_mode_button/run.sh "A remove" # filter by case name substring

Compiler: g++ >= 13 or clang++ >= 17 with C++23 (or `pip install ziglang`, used automatically).

How it works: `extract.py` copies the REAL function bodies from
`EmuFramework/src/vcontrols/VController.cc` / `VControllerDuo.cc` into `gen/real_code.inc`
(see SPECS in extract.py). They are compiled against minimal stand-ins in `stubs.hh` and the
`VController` shell in `duo_mode_button_test.cc` (no Imagine, no window, no Android).

Real: configure, resetUIGroups, writeUIButtonsConfig, the UI_BUTTONS_V2 case of readConfig,
readVControllerElement, readDuoExtra, writeToConfig, configDataSizeBytes, syncDuoModeButton,
setDuoPosition, setTwoPlayerControls, setDuoLayout.
Simulated: defaultUIGroups contents, TouchConfigView add/remove, graphics, file IO (byte buffer).

If a fix renames/moves one of these functions or adds a new config key/new member,
extract.py fails loudly or the build breaks: update SPECS / the VController shell
(new members) / loadAndConfigure (new key) accordingly.

Helpers for new cases: makeDefaultController, launch, findDuoButton, countDuoButtons,
removeDuoButton, addDuoButtonBack, setDuoButtonPosMM, setDuoButtonState, saveUIButtons,
loadAndConfigure, relaunch, oldConfigBlobWithoutDuoButton, defaultDuoButton, resetUI,
setTwoPlayer, toggleJointSeparate. Register a case by adding a line to CASES.

## Version detection (PR #2 and later)
extract.py also extracts OPTIONAL symbols when present (isDuoModeKey, uiButtonDuoExtraBytes,
writeDuoExtra, uiElementIsDuoModeButton, noteUIButtonRemoved/Added, onLayoutEditorClosed,
ensureDuoModeButton) and VController::remove (both trees), probes the source for
duoModeButtonRemoved_, DuoModeButton.hh (included as the REAL header), virtual View::onDismiss and
the TouchConfigView/PlaceVControlsView onDismiss -> onLayoutEditorClosed wiring, and writes
`gen/features.h` with HAVE_* defines. The same test files build on master and on PR #2.

- removeDuoButton / removeAllUIButtons go through the real VController::remove().
- addDuoButtonBack calls the real noteUIButtonAdded() when it exists (TouchConfigView does).
- oldConfigBlobWithoutDuoButton strips the PR #2 trailer (u8 removed + 5 bytes/element).
- loadAndConfigure passes only the key-113 payload (like the real per-key subView) and fails
  if readConfig leaves bytes unread.
- F runs only when onLayoutEditorClosed() exists: "kill" after leaving the editor and reload
  from what saveConfigFile wrote. Otherwise SKIP (manual step F).

## p5 additions

- **Case I (known issue):** remove all UI buttons -> relaunch -> add a non-Mode UI group -> relaunch; the
  Mode button must stay removed. On PR #2 (bit 7 of `duoFlags` needs a remaining UI button to live on) it
  comes back, so I is marked known issue: printed as `XFAIL` (or `XPASS` once fixed) on its own summary
  line (`known issues (not counted): N xfail, N xpass`) and never changes the exit code. A–H are unchanged.
- **`DUO_DUMP_DIR=<dir>`:** every key-113 save made with the real `writeUIButtonsConfig` is written as a
  full config file (`0x02` header + the 113 block) that `tools/neocfg.py decode` reads:
  `<case>-NN.cfg` per save, `<case>-before.cfg` = first save, `<case>-after.cfg` = last save (after
  reload + save again). Relative dirs are from the caller's cwd, e.g.
  `DUO_DUMP_DIR=tests/duo_mode_button/build/dumps tests/duo_mode_button/run.sh`
  then `python3 tests/duo_mode_button/tools/neocfg.py decode tests/duo_mode_button/build/dumps/A-after.cfg`.
  Note: in the harness's fresh install the Mode button is the only UI group, so after removing it the
  saved layout has 0 elements and no `MODE_REMOVED(bit7)` carrier appears (A/G/H/I-before all 0 elements).
- **Stub fidelity:** `KeyInfo` (4 bytes), `AppKeyCode` values and `_2DOrigin::pack()` values match the
  real headers so dumps are byte-compatible. `EmuApp::saveConfigFile` access mirrors the real
  `EmuApp.hh` of the tree under test (both overloads; `extract.py` reads the nearest access specifier ->
  `HAVE_SAVE_CONFIG_FILE_PUBLIC` / `HAVE_SAVE_CONFIG_FILE_FILEIO_PUBLIC`). So 5d49d12 fails to compile with
  `VControllerDuo.cc:238: 'saveConfigFile' is a protected member`, like the Android build; 7329d57 builds.
- **Flags:** `-std=c++23 -O0 -g -Wall -Werror -Wno-unused` (`-Werror` like the project; quiet on all trees).
- **tools/:** `neocfg.py` from teammate 3 (PR #2 5d49d12 format, `MODE_REMOVED(bit7)`), samples in
  `tools/samples-pr2b/`.
