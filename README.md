# Emu-SoloSquad

Emu-SoloSquad is a set of Android emulator forks of [Rakashazi's emu-ex-plus-alpha](https://github.com/Rakashazi/emu-ex-plus-alpha) (commit `1c12fac`). One person can drive several players from a single touchscreen.

The latest published build is [Saturn Quad v1.2](https://github.com/sevesghost/Emu-SoloSquad/releases/latest).

| App | System | Players on one screen | App id | Launcher name |
|---|---|---|---|---|
| NEO-Duo | Neo Geo | 2 (Joint or Separate), or the original one-player layout | `com.sevesghost.neoduo` | NEO-Duo |
| Saturn Quad 4P | Sega Saturn | 1, 2, or 4 | `com.sevesghost.saturnquad` | Saturn Quad |

BIOS images and game ROMs are not included. You supply your own legally obtained files.

## NEO-Duo

NEO-Duo is a Neo Geo emulator for Android. It installs next to the original NEO.emu package.

### What you can do

On a fresh install the on-screen controls start in **two-player** mode, using the **Joint** layout:

- One set of buttons (D-pad, A–D, turbo A–D, A+B, C+D, A+B+C, and the turbo combos) sends each press to **both** players. In landscape the D-pad stays on the left and the face buttons sit in two rows on the right.
- Small per-player Coin and Start buttons sit in the top corners. Joint mode also has a white shared Select and Start at the top center.
- **Separate** mode puts a blue Player 1 set on the left and a mirrored red Player 2 set on the right. An optional row of white buttons at the bottom center can drive both players at once (off by default) and stays at least 12 mm clear of the player buttons.
- A Mode button at the top of the screen switches Joint and Separate when it is held for half a second. While two-player mode is on, the old single-player Virtual Gamepad Player option is greyed out.
- Turning **One Person, Both Players** off restores the original single-player touch layout. Turning it back on brings the Mode button back.
- A removed Mode button stays removed across relaunch, and a moved or hidden Mode button keeps that position and hidden state, while two-player mode stays on. There is no extra config key: the removal is bit 7 of the on-screen UI-button trailer. **Reset UI Groups** clears the removal and puts a shown Mode button back at the default position.
- Each set has its own size and opacity, including the shared buttons. Player 1 and Player 2 start at 50% opacity and the shared row at 40%. Lock P1/P2 Size keeps the two face sizes equal. Changing a size leaves dragged positions where they are. Buttons never shrink below 9 mm; a tight screen packs the gaps instead. **Reset Emulator Button Positions** puts the current mode back on its default anchors.
- Layouts are stored in millimetres from the screen edges under config key `CFGKEY_VCONTROLLER_DEVICE_BUTTONS_V3` (`configId` 0 = separate, 1 = joint), with a separate landscape and portrait arrangement. The turbo row stays inside the bottom 40% of the screen. Wider than 16:10 keeps the controls over the picture. 16:10 or narrower shrinks the game image and leaves a strip along the bottom.
- The game picture can be dragged anywhere on the screen from **Set Video Position**, with Smaller/Larger controls and a reset, or positioned from Video settings (horizontal offset, vertical offset, and placement scale). Landscape and portrait remember their own offsets and scale. The touch pads stay drawn above the picture.
- Launching the app mixes with other audio (YouTube, music) instead of taking exclusive audio focus. That default is not switched back on later.
- Leaving the touch setup with Back writes the config immediately. Pausing or exiting the app writes it again.

### Save states

The 68000 core is now [Musashi 4.x](https://github.com/kstenerud/Musashi) (MIT) on every ABI, including 32-bit ARM. The Z80 core is [MAME's current Z80](https://github.com/mamedev/mame/tree/master/src/devices/cpu/z80) (BSD-3-Clause), also on every ABI. The old non-commercial Musashi 3.32, Portable Z80 3.9, and GPL-2-only Cyclone cores are gone.

Because of that, **save states written by the previous cores do not load**. The state header uses new core flags (`m68k` 0x6, `z80` 0x9) and rejects older files.

## Saturn Quad 4P

Saturn Quad is the Saturn.emu shell with four on-screen control sets. The launcher name is **Saturn Quad** and the app id is `com.sevesghost.saturnquad`, so it installs next to the original Saturn.emu package. Saves and states in Saturn.emu do not move across by themselves. [v1.2 release notes](https://github.com/sevesghost/Emu-SoloSquad/releases/tag/v1.2) describe how to point both apps at the same save folder.

On-screen Input Setup shows **On-screen Players** with **1 Player**, **2 Players**, and **4 Players**. A fresh install starts at 4. A saved config that has no player-count byte stays at 2 players (or 1, if it was a one-player save) and does not turn the multitap on.

- Each player has a touch set: P1 blue, P2 red, P3 green, P4 amber. A press on one set goes to that player's port.
- At 1 or 2 players, P3 and P4 stay in the config but are not drawn or hit-tested. Raising the count lays out only the new sets, so a dragged P1 or P2 layout stays where it is. Dropping the count keeps positions and hidden-button state. One-player mode still saves every set.
- Four on-screen players turn on the multitap in controller port 1 and set that port's four slots to gamepads. There is no 3-player choice. Dropping below 4 turns that multitap off only when this feature turned it on. A Port 1 multitap you enabled yourself in the input menu stays on. Port 2 is left alone.
- The Mode button (Toggle Control Layout) is shown at 1, 2, and 4 players unless you removed it. Hold it for half a second to switch Joint and Separate. That switch does not change the player count and does not clear a removal.
- Removal, position, and hidden state are saved with the other UI buttons. Removal is bit 7 on the UI-button trailer, not a new config key. Changing 1 / 2 / 4 does not bring a removed Mode button back. **Reset UI Groups** clears the removal and puts a shown Mode button back at the default spot, together with the default UI groups.
- Back out of On-screen Input Setup (or out of the screen where you place the controls) writes the config before the screen closes. Pausing or exiting the app writes it again. [v1.2](https://github.com/sevesghost/Emu-SoloSquad/releases/tag/v1.2) still says to press Back out of setup before closing, so the edit is stored even if the process is killed first.
- v1.2 is signed with the Android debug key. If you installed an earlier test build, uninstall it first: it was signed with a different certificate, and uninstalling clears that install's saved settings.

The v1.2 notes also record a limit of that release: in-game checks (P3/P4 during play, the multitap with a disc, hidden buttons during play, soft/hard reset, and loading a game) were not run, because no BIOS or games were available. The on-screen setup checks for the four button bugs above did pass.

## Credits and licenses

Emu-SoloSquad as a whole is **GPL-3.0-or-later**, the same license as Imagine, EmuFramework, NEO.emu, and Saturn.emu. See `COPYING.GPL`.

Upstream:

- Imagine and EmuFramework, and the NEO.emu and Saturn.emu application shells — © Robert Broglia, GPL-3.0-or-later. <https://github.com/Rakashazi/emu-ex-plus-alpha>
- Gngeo — © the Gngeo team, GPL-2.0-or-later. The Neo Geo machine code under `NEO.emu/src/gngeo` comes from that project. The ARM and x86 blitters (`video_arm.s`, `video_i386.asm`) are not in this tree; every ABI uses the C blitter.
- 68000 — Musashi © Karl Stenerud, MIT. Sources are in `NEO.emu/src/musashi`. The 68000 has no FPU or PMMU, so `m68kfpu.c`, `m68kmmu.h`, and SoftFloat are not included.
- Z80 — MAME's ZiLOG Z80 core, BSD-3-Clause, © Juergen Buchmueller and the MAME team. Sources and the generated `z80.hxx` are in `NEO.emu/src/z80`. Regenerate the execution loop with `python3 z80make.py z80.lst z80.hxx` from that directory if you update `z80.lst`.
- YM2610 (`ym2610.c`) is GPL-2.0-or-later, © Jarek Burczynski and Tatsuyuki Satoh, from MAME's `fm.cpp` and `ymdeltat.cpp`. The interface in `2610intf.c` is BSD-3-Clause © Ernesto Corvi plus the Gngeo GPL-2.0-or-later integration. Neo Geo protection (`neocrypt.c`) stays under MAME's BSD-3-Clause terms. None of these were rewritten onto ymfm.
- The calendar chip in `pd4990a.c` is an original implementation. Its shift register and strobe follow MAME's BSD-3-Clause `upd1990a` device (© Curt Coder, hap, and the MAME team). The old non-commercial MAME `pd4990a` sources are not in this tree.
- Mednafen — © the Mednafen Team, GPL-2.0-or-later. Saturn.emu's machine code is under `Saturn.emu/src/ss`. The shared library `mednafen_common` is built from `EmuFramework/src/shared` (Mednafen sources plus the `mednafen-emuex` glue) and Saturn.emu links it.
- LZMA — Igor Pavlov's LZMA SDK, public domain, under `EmuFramework/src/shared/lzma`. CHD images go through this decoder.
- libchdr — MAME Compressed Hunks of Data, BSD-3-Clause, © Aaron Giles, under `EmuFramework/src/shared/libchdr`.
- The image shaders in `EmuFramework/res/shaders` (direct, scale, and the edge-directed smoother, including the fallback variants) are original GPL-3.0-or-later code. The previous unlicensed hq2x and Scale2x shader text was replaced.
- The 4-clause BSD `memcpy.S` and the unused zlib `crypt.h` are not in this tree. BTstack (non-commercial) is not built or shipped; Android uses the platform Bluetooth adapter.
- Other bundled libraries are listed in `THIRD-PARTY-NOTICES`.

No BIOS, ROMs, keystores, or signing passwords are in this repository.

## Building a signed Android APK

Host tools used for the release build: JDK 21, Android SDK platform 36, Android NDK r29, and the NDK's Clang (21+). CMake is fetched by the project scripts.

```bash
export ANDROID_SDK_ROOT="$HOME/android-sdk"   # must contain platforms/android-36 and an NDK
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export IMAGINE_PATH="$PWD/imagine"
export EMUFRAMEWORK_PATH="$PWD/EmuFramework"
export IMAGINE_SDK_PATH="${IMAGINE_SDK_PATH:-$HOME/imagine-sdk}"

# Imagine's third-party libraries (all ABIs the meta build lists: armv7, arm64, x86, x86_64)
( cd imagine/bundle/all && ./makeAll-android.sh install )

# Point the makefiles at this tree
./imagine/android.sh config
./imagine/android.sh installLinks --config Release
./EmuFramework/android.sh config
./EmuFramework/android.sh installLinks --config Release

# Release signing. Keep the keystore and passwords outside the repo.
# ~/.gradle/gradle.properties (mode 600) — do not commit this file:
#   ANDROID_KEY_STORE=/path/to/release.keystore
#   ANDROID_KEY_STORE_PASSWORD=...
#   ANDROID_KEY_ALIAS=...
#   ANDROID_KEY_PASSWORD=...
# Without those properties the release build is signed with the debug key.

cd NEO.emu
make -f android.mk android-apk CONFIG=Release

cd ../Saturn.emu
make -f android.mk android-apk CONFIG=Release
```

NEO-Duo's universal APK is `NEO.emu/build/android/build/outputs/apk/release/NeoDuo-release.apk` (`arm64-v8a`, `armeabi-v7a`, `x86_64`, and `x86`). Saturn Quad's APK is `Saturn.emu/build/android/build/outputs/apk/release/SaturnEmu-release.apk`. `Saturn.emu/config.mk` sets `android_arch` to `arm64` and `x86_64`. BIOS and game archives are not packaged. The file published as v1.2 is `Saturn-Quad-v1.2.apk`.

## ภาษาไทย

Emu-SoloSquad เป็นชุดแอป Android ที่แยกมาจาก emu-ex-plus-alpha ของ Rakashazi ให้คนคนเดียวควบคุมผู้เล่นหลายคนจากจอสัมผัสจอเดียว

| แอป | เครื่อง | ผู้เล่นบนจอเดียว | รหัสแอป | ชื่อบนหน้าจอ |
|---|---|---|---|---|
| NEO-Duo | Neo Geo | 2 คน (Joint หรือ Separate) หรือเลย์เอาต์ผู้เล่นคนเดียวแบบเดิม | `com.sevesghost.neoduo` | NEO-Duo |
| Saturn Quad 4P | Sega Saturn | 1, 2 หรือ 4 คน | `com.sevesghost.saturnquad` | Saturn Quad |

รุ่นที่เผยแพร่ล่าสุดคือ [Saturn Quad v1.2](https://github.com/sevesghost/Emu-SoloSquad/releases/latest)

NEO-Duo ติดตั้งคู่กับ NEO.emu เดิมได้ ติดตั้งใหม่จะเริ่มที่โหมดสองผู้เล่นแบบ Joint ปุ่ม Mode กดค้างครึ่งวินาทีสลับ Joint กับ Separate ถ้าลบ ย้าย หรือซ่อนปุ่ม Mode ไว้ ค่านั้นอยู่หลังเปิดแอปใหม่เมื่อยังเปิดโหมดสองผู้เล่น Reset UI Groups นำปุ่ม Mode กลับมา คอร์ 68000 และ Z80 เปลี่ยนแล้ว เซฟสเตทรุ่นเก่าเปิดไม่ได้ ไม่มี BIOS หรือรอมในแอป

Saturn Quad ติดตั้งคู่กับ Saturn.emu เดิมได้ เซฟไม่ย้ายตามเอง เลือกจำนวนผู้เล่นที่ On-screen Players เป็น 1, 2 หรือ 4 ติดตั้งใหม่เริ่มที่ 4 คน คอนฟิกเก่าที่ไม่มีจำนวนผู้เล่นจะอยู่ที่ 2 คนและไม่เปิด multitap ปุ่ม P1–P4 คนละสี กดชุดไหนไปพอร์ตนั้น ที่ 1 หรือ 2 คน ปุ่ม P3/P4 ยังถูกเก็บไว้แต่ไม่แสดงและกดไม่ได้ สี่ผู้เล่นเปิด multitap ที่พอร์ต 1 ลดจำนวนผู้เล่นแล้วจะปิดเฉพาะ multitap ที่ฟีเจอร์นี้เปิดเอง multitap ที่เปิดจากเมนูเองจะค้างไว้ ปุ่ม Mode แสดงทั้ง 1, 2 และ 4 คนจนกว่าจะลบ กดค้างครึ่งวินาทีสลับเลย์เอาต์ ไม่ได้เปลี่ยนจำนวนผู้เล่น การลบ ตำแหน่ง และการซ่อนถูกบันทึกไว้ การเปลี่ยนจำนวนผู้เล่นไม่ทำให้ปุ่ม Mode ที่ลบไปกลับมา Reset UI Groups ทำให้ปุ่ม Mode กลับมา กด Back ออกจากหน้าตั้งปุ่มแล้วค่าถูกเขียนทันที การพักหรือปิดแอปก็เขียนอีกครั้ง v1.2 เซ็นด้วย debug key ถ้าเคยลงรุ่นทดสอบเก่าให้ถอนก่อนเพราะใบรับรองคนละตัว

ทั้งโปรเจกต์เป็น GPL-3.0-or-later แกน Saturn มาจาก Mednafen (GPL-2.0-or-later) CHD ใช้ libchdr (BSD-3-Clause) และ LZMA (public domain) ไม่มี BIOS, รอม, keystore หรือรหัสผ่านอยู่ในที่เก็บนี้
