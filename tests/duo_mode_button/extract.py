#!/usr/bin/env python3
"""Copy REAL function bodies out of the EmuFramework sources so the host test
compiles the code that ships, not a hand-written copy.

Each spec: (source file, start-text, kind)
  kind "func": the definition starting at start-text, through its matching '}'.
  kind "case": a `case X:` label followed by its `{...}` block (wrapped by the test).
If a spec is not found (e.g. a fix renames a function) the script fails loudly so
the test is updated instead of silently testing nothing.
"""
import sys, pathlib
root = pathlib.Path(sys.argv[1]); out = pathlib.Path(sys.argv[2])
VC  = 'EmuFramework/src/vcontrols/VController.cc'
VCD = 'EmuFramework/src/vcontrols/VControllerDuo.cc'
# (file, start-text, kind, required). Optional specs are emitted only if present
# (e.g. symbols added by PR #2) and each found optional spec defines HAVE_<name>
# in gen/features.h so the harness adapts without hardcoding a version.
SPECS = [
	(VCD, 'static bool isDuoModeKey(', 'func', False, 'IS_DUO_MODE_KEY'),
	(VC,  'constexpr size_t uiButtonDuoExtraBytes', 'stmt', False, 'UI_DUO_EXTRA_BYTES'),
	(VC,  'static bool readVControllerElement(', 'func', True, None),
	(VC,  'static bool readDuoExtra(', 'func', True, None),
	(VC,  'static void writeDuoExtra(', 'func', False, 'WRITE_DUO_EXTRA'),
	(VC,  'static void writeToConfig(const VControllerElement', 'func', True, None),
	(VC,  'static size_t configDataSizeBytes(', 'func', True, None),
	(VC,  'void VController::writeUIButtonsConfig(', 'func', True, None),
	(VC,  'void VController::configure(', 'func', True, None),
	(VC,  'void VController::resetUIGroups()', 'func', True, None),
	(VC,  'bool VController::remove(VControllerElement', 'func', True, None),
	(VCD, 'void VController::setDuoPosition(', 'func', True, None),
	(VCD, 'bool VController::uiElementIsDuoModeButton(', 'func', False, 'UI_ELEMENT_IS_DUO_MODE_BUTTON'),
	(VCD, 'void VController::noteUIButtonRemoved(', 'func', False, 'NOTE_UI_BUTTON_REMOVED'),
	(VCD, 'void VController::noteUIButtonAdded(', 'func', False, 'NOTE_UI_BUTTON_ADDED'),
	(VCD, 'void VController::onLayoutEditorClosed()', 'func', False, 'ON_LAYOUT_EDITOR_CLOSED'),
	(VCD, 'void VController::ensureDuoModeButton(', 'func', False, 'ENSURE_DUO_MODE_BUTTON'),
	(VCD, 'void VController::syncDuoModeButton()', 'func', True, None),
	(VCD, 'void VController::setTwoPlayerControls(', 'func', True, None),
	(VCD, 'void VController::setDuoLayout(', 'func', True, None),
	(VC,  'case CFGKEY_VCONTROLLER_UI_BUTTONS_V2:', 'case', True, None),
]
# Plain text probes (source wiring, not compiled): define HAVE_<name> if found.
PROBES = [
	('EmuFramework/include/emuframework/VController.hh', 'duoModeButtonRemoved_', 'DUO_REMOVED_FLAG'),
	('EmuFramework/include/emuframework/DuoModeButton.hh', 'decideDuoModeButton', 'DUO_MODE_BUTTON_HH'),
	('imagine/include/imagine/gui/View.hh', 'virtual void onDismiss()', 'VIRTUAL_ON_DISMISS'),
	('EmuFramework/src/gui/TouchConfigView.cc', 'void TouchConfigView::onDismiss()\n{\n\tvController.onLayoutEditorClosed();', 'TOUCHCONFIG_DISMISS_SAVES'),
	('EmuFramework/src/gui/PlaceVControlsView.cc', 'void PlaceVControlsView::onDismiss()\n{\n\tvController.onLayoutEditorClosed();', 'PLACEVCONTROLS_DISMISS_SAVES'),
]
def block(text, start):
	i = text.find('{', start); depth = 0
	for j in range(i, len(text)):
		if text[j] == '{': depth += 1
		elif text[j] == '}':
			depth -= 1
			if depth == 0: return text[start:j+1]
	raise SystemExit('unbalanced braces after ' + repr(text[start:start+60]))
parts = []; feats = []
for f, pat, kind, required, feat in SPECS:
	text = (root / f).read_text()
	idx = text.find(pat)
	if idx < 0:
		if required: raise SystemExit(f'extract.py: "{pat}" not found in {f} -- update SPECS')
		continue
	if kind == 'stmt': body = text[idx:text.index(';', idx)+1]
	else: body = block(text, idx)
	line = text.count('\n', 0, idx) + 1
	if kind == 'case':
		body = ('bool VController::readUIButtonsConfigCase(EmuApp &app, MapIO &io)\n{\n'
			'\tswitch(CFGKEY_VCONTROLLER_UI_BUTTONS_V2)\n\t{\n' + body + '\n\t}\n\treturn false;\n}')
	if feat: feats.append(feat)
	parts.append(f'// ---- REAL CODE from {f}:{line} ----\n#line {line} "{f}"\n{body}\n')
for f, pat, feat in PROBES:
	pth = root / f
	if pth.exists() and pat in pth.read_text(): feats.append(feat)
# Mirror the REAL access level of EmuApp::saveConfigFile(ApplicationContext): find the declaration in
# EmuApp.hh and the nearest preceding access specifier. stubs.hh uses it, so code calling a protected
# saveConfigFile fails to compile here exactly like the Android -Werror build.
import re as _re
_emu = (root / 'EmuFramework/include/emuframework/EmuApp.hh').read_text()
# Both overloads are probed: (ApplicationContext) -> HAVE_SAVE_CONFIG_FILE_PUBLIC, (FileIO &) -> HAVE_SAVE_CONFIG_FILE_FILEIO_PUBLIC.
for _sig, _feat in ((r'ApplicationContext', 'SAVE_CONFIG_FILE_PUBLIC'), (r'FileIO\s*&', 'SAVE_CONFIG_FILE_FILEIO_PUBLIC')):
	_m = _re.search(r'void\s+saveConfigFile\s*\(\s*' + _sig + r'[^)]*\)\s*;', _emu)
	if not _m: sys.exit(f'extract.py: saveConfigFile({_sig}) not found in EmuApp.hh')
	_acc = _re.findall(r'^\s*(public|protected|private)\s*:', _emu[:_m.start()], _re.M)
	if _acc and _acc[-1] == 'public': feats.append(_feat)
hdr = '#pragma once\n' + ''.join(f'#define HAVE_{x} 1\n' for x in feats)
if 'DUO_MODE_BUTTON_HH' in feats:
	hdr += '#include "' + str(root / 'EmuFramework/include/emuframework/DuoModeButton.hh') + '" // REAL header\n'
(out.parent / 'features.h').write_text(hdr)
out.write_text('\n'.join(parts))
print(f'extracted {len(parts)} real code blocks -> {out}; features: {" ".join(feats) or "(none: master)"}')
