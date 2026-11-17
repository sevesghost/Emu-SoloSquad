# Duo Mode button (ปุ่มสามขีด / `toggleDuoLayout`) — manual test steps

Device or Android emulator, NEO-Duo APK built from the branch under test.
"Relaunch" = swipe the app away from Recents (force stop) and open it again.
Layout editor = in-game menu → Options → Input → On-screen Input Setup (TouchConfigView) → Set Button Positions / element list.
Mode button = the three-line icon (`AssetID::menu`), default top-centre ~30 mm down, two-player mode on.

Before each case: Options → On-screen Input Setup → **Reset UI Groups** (or clear app data), relaunch.

| Case | Steps | Expected (after fix) | `master` 9350574 |
|---|---|---|---|
| A remove | Layout editor → UI buttons → Mode button → **Remove This Button** → back out of the editor → relaunch | Mode button is gone. Relaunch again: still gone | button comes back (bug) |
| B add back | After A: Layout editor → **Add Button** → choose the Mode/Toggle Control Layout button → drag it to a corner, set State = Hidden → back out → relaunch | Exactly one Mode button, at the dragged position, still Hidden | button back at default place, Shown |
| C move/hide | Drag the Mode button to a new place in **landscape**, rotate to **portrait**, drag it elsewhere, set State = Hidden → back out → relaunch, check both orientations | Same place in each orientation (not swapped, not scaled wrong), same size, still Hidden | resets to default |
| D Reset UI Groups | After A (button removed) → On-screen Input Setup → **Reset UI Groups** → relaunch | Mode button back at default position/size, Shown | passes |
| E old config | Install an older build (before the Mode button) or use the sample config from (ตก) 3 without the button in key 113 → upgrade to test build → launch | Mode button shown with defaults | shown (passes) |
| F save on leaving editor | Remove (or move) the Mode button → leave the layout editor with Back → **immediately** `adb pull` the config and check it with `python3 tests/duo_mode_button/tools/neocfg.py decode <config>` (Mode button already removed/moved in key `113`) → `adb shell am force-stop <pkg>` (do not go through the main menu / exit) → relaunch → `adb pull` again and `neocfg.py diff <before> <after>` | change is kept (config written when leaving the editor, before the kill, not only on app exit) | change lost |

F is automated on host for trees with `onLayoutEditorClosed()` (PR #2); still run the manual step on device. Earlier note: `TouchConfigView` is a GUI View and cannot be built on host. If the fix adds a callable seam
(e.g. `VController::onLayoutEditorClosed()` / a `saveConfigNow` hook), an automated case can be added.

## Not fixed this round — notes only (do NOT fail the PR on these)
1. **Two-player off → on** (`setTwoPlayerControls`): by design (north's decision) this calls `syncDuoModeButton()` and the Mode button **comes back** even if the user removed it. Known behaviour, not a bug for this PR.
2. **Joint ↔ Separate** (`setDuoLayout`): now covered by automated case **G** (remove → switch → relaunch must stay removed). Manual check on device: same steps, also switch back once.
3. **Rewind = 0**: rewind button is hidden via `uiKeyIsEnabled()` (not removed). Set rewind states = 0 → remove Mode button → relaunch → set rewind > 0: rewind button must come back; Mode button should stay removed. Record result.
4. **v1 config (sentinel `-1` positions)**: `configure()` calls `resetUIGroups()`; Mode button should show with defaults. Needs a real v1 config file (sample from (ตก) 3). The "remove all UI buttons must not crash" part is automated case **H**.
