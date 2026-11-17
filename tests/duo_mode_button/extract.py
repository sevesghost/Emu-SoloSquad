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
SPECS = [
	(VC,  'static bool readVControllerElement(', 'func'),
	(VC,  'static bool readDuoExtra(', 'func'),
	(VC,  'static void writeToConfig(const VControllerElement', 'func'),
	(VC,  'static size_t configDataSizeBytes(', 'func'),
	(VC,  'void VController::writeUIButtonsConfig(', 'func'),
	(VC,  'void VController::configure(', 'func'),
	(VC,  'void VController::resetUIGroups()', 'func'),
	(VCD, 'void VController::setDuoPosition(', 'func'),
	(VCD, 'void VController::syncDuoModeButton()', 'func'),
	(VCD, 'void VController::setTwoPlayerControls(', 'func'),
	(VCD, 'void VController::setDuoLayout(', 'func'),
	(VC,  'case CFGKEY_VCONTROLLER_UI_BUTTONS_V2:', 'case'),
]
def block(text, start):
	i = text.find('{', start); depth = 0
	for j in range(i, len(text)):
		if text[j] == '{': depth += 1
		elif text[j] == '}':
			depth -= 1
			if depth == 0: return text[start:j+1]
	raise SystemExit('unbalanced braces after ' + repr(text[start:start+60]))
parts = []
for f, pat, kind in SPECS:
	text = (root / f).read_text()
	idx = text.find(pat)
	if idx < 0: raise SystemExit(f'extract.py: "{pat}" not found in {f} -- update SPECS')
	body = block(text, idx)
	line = text.count('\n', 0, idx) + 1
	if kind == 'case':
		body = ('bool VController::readUIButtonsConfigCase(EmuApp &app, MapIO &io)\n{\n'
			'\tswitch(CFGKEY_VCONTROLLER_UI_BUTTONS_V2)\n\t{\n' + body + '\n\t}\n\treturn false;\n}')
	parts.append(f'// ---- REAL CODE from {f}:{line} ----\n#line {line} "{f}"\n{body}\n')
out.write_text('\n'.join(parts))
print(f'extracted {len(parts)} real code blocks -> {out}')
