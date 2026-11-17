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
