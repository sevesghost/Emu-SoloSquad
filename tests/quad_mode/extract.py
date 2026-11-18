#!/usr/bin/env python3
"""Probe the tree under test for the 4-player implementation and write gen/features.h.
HAVE_QUAD_IMPL is defined only when ALL required symbols exist; otherwise QUAD_MISSING lists
what is missing and the real backend reports every case PENDING (never a fake pass).
When the implementation lands, west adds the REAL-code extraction (same method as
tests/duo_mode_button/extract.py of NEO-Duo) and core/backend_real_impl.inc."""
import sys, pathlib, re
root = pathlib.Path(sys.argv[1]); gen = pathlib.Path(sys.argv[2]); gen.mkdir(parents=True, exist_ok=True)
VC='EmuFramework/src/vcontrols/VController.cc'; VCD='EmuFramework/src/vcontrols/VControllerDuo.cc'
VH='EmuFramework/include/emuframework/VController.hh'
# (file, text, feature, required)
PROBES = [
	(VCD, 'void VController::syncDuoModeButton()', 'SYNC_DUO_MODE_BUTTON', True),
	(VCD, 'void VController::noteUIButtonRemoved(', 'NOTE_UI_BUTTON_REMOVED', True),
	(VCD, 'void VController::noteUIButtonAdded(', 'NOTE_UI_BUTTON_ADDED', True),
	(VCD, 'void VController::onLayoutEditorClosed()', 'ON_LAYOUT_EDITOR_CLOSED', True),
	(VCD, 'void VController::setDuoLayout(', 'SET_DUO_LAYOUT', True),
	(VCD, 'void VController::setPlayerControls(', 'SET_PLAYER_CONTROLS', True),
	('EmuFramework/include/emuframework/QuadControls.hh', 'maxDuoPlayers', 'MAX_DUO_PLAYERS', True),
	('EmuFramework/include/emuframework/QuadControls.hh', 'duoRoleColor', 'DUO_ROLE_COLOR', True), # PR #4 names it duoRoleColorRgba (QuadControls.hh:57)
	('EmuFramework/include/emuframework/QuadControls.hh', 'duoRolePort(', 'DUO_ROLE_PORT', True),
	(VC,  'constexpr size_t uiButtonDuoExtraBytes', 'UI_DUO_EXTRA_BYTES', False), # <=6f5c530: VController.cc:27 (extern constexpr; breaks NDK clang)
	('EmuFramework/include/emuframework/VController.hh', 'inline constexpr size_t uiButtonDuoExtraBytes', 'UI_DUO_EXTRA_BYTES', False), # NDK fix: defined in header
	('Saturn.emu/src/main/input.cc', 'maxPlayers', 'SATURN_INPUT', False),
]
feats=[]; missing=[]
for f, pat, feat, req in PROBES:
	p = root / f
	if p.exists() and pat in p.read_text(errors='replace'): feats.append(feat)
	elif req: missing.append(f'{pat.strip("(")} [{f}]')
if 'UI_DUO_EXTRA_BYTES' not in feats: missing.append('uiButtonDuoExtraBytes [VController.cc or VController.hh]')
feats = list(dict.fromkeys(feats))
hdr = '#pragma once\n' + ''.join(f'#define HAVE_{x} 1\n' for x in feats)
if not missing and (gen.parent / 'core/backend_real_impl.inc').exists(): hdr += '#define HAVE_QUAD_IMPL 1\n'
hdr += '#define QUAD_MISSING "' + ('; '.join(missing) or ('backend_real_impl.inc not written yet' if not missing else '')).replace('"', "'") + '"\n'
(gen / 'features.h').write_text(hdr)
print(f'probe {root}: found {" ".join(feats) or "(none)"}; missing {len(missing)}')
