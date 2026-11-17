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
