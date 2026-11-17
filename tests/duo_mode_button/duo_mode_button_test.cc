// Host-side regression tests for the Duo "Mode" button (AppKeyCode::toggleDuoLayout,
// three-line icon AssetID::menu) persisting across app restarts.
//
// REAL code (copied verbatim at build time by extract.py into gen/real_code.inc):
//   VController::configure, resetUIGroups, writeUIButtonsConfig, syncDuoModeButton,
//   setDuoPosition, setTwoPlayerControls, setDuoLayout, the CFGKEY_VCONTROLLER_UI_BUTTONS_V2
//   case of readConfig, and the static readVControllerElement / readDuoExtra /
//   writeToConfig / configDataSizeBytes.
// SIMULATED (stubs.hh + the VController shell below): Imagine types, graphics,
//   defaultUIGroups() contents, add() from TouchConfigView, file IO (byte buffer).
//
// Adding a case: write `static void caseX(Ctx &t)` using the helpers, then add one line
// to CASES at the bottom. Use t.check(cond, "msg"). Return early after a failed check
// if later steps would crash.
#include "stubs.hh"
#include "gen/features.h" // HAVE_* flags from extract.py (which symbols this source tree has)
#include <cstdio>
#include <functional>
#include <string>

namespace EmuEx
{

class VController
{
public:
	EmuApp app_;
	std::vector<VControllerElement> uiElements{};
	std::vector<VControllerElement> gpElements{};
	bool twoPlayer_{true};
	bool duoCustomized_{};
	DuoLayout duoLayout_{DuoLayout::Joint};
	bool hasWindow_{};
	FanQuadIdxs fanQuadIdxs;
	int gamepadTex{}, uiTex{};
	int resetEmulatedDeviceGroupsCalls{};
#ifdef HAVE_DUO_REMOVED_FLAG
	bool duoModeButtonRemoved_{};
#endif

	// --- simulated environment ---
	EmuApp &app() { return app_; }
	const EmuApp &app() const { return app_; }
	bool hasWindow() const { return hasWindow_; }
	void setWindow(Window &) { hasWindow_ = true; }
	void setRenderer(Gfx::Renderer &) {}
	void setFace(const Gfx::GlyphTextureSet &) {}
	void update(VControllerElement &) const {}
	void place() {}
	void setInputPlayer(int) {}
	int appContext() const { return 0; }
	void resetEmulatedDeviceGroups() { resetEmulatedDeviceGroupsCalls++; gpElements.clear(); gpElements.emplace_back(std::in_place_type<VControllerButtonGroup>); gpElements[0].layoutPos[0].pos = {10, 10}; }
	// SIMULATED: real one uses add(rightUIComponents/leftUIComponents) + resetUIPositions
	std::vector<VControllerElement> defaultUIGroups() const
	{
		std::vector<VControllerElement> v;
		v.emplace_back(std::in_place_type<VControllerUIButtonGroup>, VControllerUIButtonGroup::Config{{KeyInfo::appKey(AppKeyCode::openSystemActions)}, {1, RT2DO}});
		v.back().layoutPos[0] = v.back().layoutPos[1] = {RT2DO, {0, 0}};
		v.emplace_back(std::in_place_type<VControllerUIButtonGroup>, VControllerUIButtonGroup::Config{{KeyInfo::appKey(AppKeyCode::fastForward), KeyInfo::appKey(AppKeyCode::rewind)}, {1, LT2DO}});
		v.back().layoutPos[0] = v.back().layoutPos[1] = {LT2DO, {0, 0}};
		return v;
	}

	// --- real code (see gen/real_code.inc) ---
	void configure(Window &, Gfx::Renderer &, const Gfx::GlyphTextureSet &);
	void resetUIGroups();
	void writeUIButtonsConfig(FileIO &) const;
	void syncDuoModeButton();
	void setDuoPosition(VControllerElement &, int layoutIdx, _2DOrigin, float xMm, float yMm);
	void setTwoPlayerControls(bool);
	void setDuoLayout(DuoLayout);
	bool readUIButtonsConfigCase(EmuApp &, MapIO &);
	bool remove(VControllerElement &);
#ifdef HAVE_ENSURE_DUO_MODE_BUTTON
	void ensureDuoModeButton(bool resetToDefault);
#endif
#ifdef HAVE_ON_LAYOUT_EDITOR_CLOSED
	void onLayoutEditorClosed();
#endif
#ifdef HAVE_NOTE_UI_BUTTON_REMOVED
	void noteUIButtonRemoved(KeyInfo);
#endif
#ifdef HAVE_NOTE_UI_BUTTON_ADDED
	void noteUIButtonAdded(KeyInfo);
#endif
#ifdef HAVE_UI_ELEMENT_IS_DUO_MODE_BUTTON
	bool uiElementIsDuoModeButton(const VControllerElement &) const;
#endif
};

#include "gen/real_code.inc"

// ============================== helpers ==============================
// (shared with the edge-case authors; keep names stable)

inline bool isDuoModeButton(const VControllerElement &e)
{
	auto *g = e.uiButtonGroup();
	return g && std::ranges::any_of(g->buttons, [](auto &b){ return b.key.flags.appCode && AppKeyCode(b.key.codes[0]) == AppKeyCode::toggleDuoLayout; });
}
inline int countDuoButtons(const VController &vc) { return (int)std::ranges::count_if(vc.uiElements, isDuoModeButton); }
inline VControllerElement *findDuoButton(VController &vc)
{
	auto it = std::ranges::find_if(vc.uiElements, isDuoModeButton);
	return it == vc.uiElements.end() ? nullptr : &*it;
}

// Simulated app launch: real configure() on whatever was loaded into vc.
inline void launch(VController &vc) { Window w; Gfx::Renderer r; Gfx::GlyphTextureSet f; vc.configure(w, r, f); }

// Fresh install: no config file at all, then launch.
inline VController makeDefaultController(bool twoPlayer = true)
{
	VController vc; vc.twoPlayer_ = twoPlayer;
	launch(vc);
	return vc;
}

// Removing the Mode button element in the editor -> REAL VController::remove()
// (on PR #2 that also records duoModeButtonRemoved_; on master it is a plain erase).
inline bool removeDuoButton(VController &vc)
{
	bool any = false;
	while(auto *e = findDuoButton(vc)) { if(!vc.remove(*e)) break; any = true; }
	return any;
}

// Remove every UI element through the REAL remove() path (case H).
inline void removeAllUIButtons(VController &vc)
{
	while(!vc.uiElements.empty()) { if(!vc.remove(vc.uiElements.front())) break; }
}

// "Add Button" in TouchConfigView: VController::add(InputComponentDesc{ui, {toggleDuoLayout}, CT2DO}).
// SIMULATED: real add() places it at the layout centre in pixels.
inline VControllerElement &addDuoButtonBack(VController &vc)
{
	auto &e = vc.uiElements.emplace_back(std::in_place_type<VControllerUIButtonGroup>,
		VControllerUIButtonGroup::Config{{KeyInfo::appKey(AppKeyCode::toggleDuoLayout)}, {1, CT2DO}});
	e.layoutPos[0] = e.layoutPos[1] = {C2DO, {0, 0}};
#ifdef HAVE_NOTE_UI_BUTTON_ADDED
	vc.noteUIButtonAdded(KeyInfo::appKey(AppKeyCode::toggleDuoLayout)); // REAL: TouchConfigView calls this on add
#endif
	return e;
}

// Move in mm with the REAL setDuoPosition (sets DUO_POS_MM). layoutIdx 0 = landscape, 1 = portrait.
inline void setDuoButtonPosMM(VController &vc, VControllerElement &e, int layoutIdx, _2DOrigin o, float xMm, float yMm)
{
	vc.setDuoPosition(e, layoutIdx, o, xMm, yMm);
}
inline void setDuoButtonState(VControllerElement &e, VControllerState s) { e.state = s; }

// Save UI buttons with the REAL writeUIButtonsConfig. Blob layout: u16 size, u16 key, payload.
inline ConfigBlob saveUIButtons(const VController &vc) { ConfigBlob b; vc.writeUIButtonsConfig(b); return b; }

// Load a blob produced by saveUIButtons into a NEW controller (real readConfig case) and launch it.
// Pass an empty blob to simulate a config file with no key 113 at all.
inline VController loadAndConfigure(ConfigBlob blob, bool twoPlayer = true, DuoLayout layout = DuoLayout::Joint)
{
	VController vc; vc.twoPlayer_ = twoPlayer; vc.duoLayout_ = layout;
	if(blob.bytes.size())
	{
		blob.rd = 0;
		blob.get<uint16_t>();
		auto key = blob.get<uint16_t>();
		if(key != CFGKEY_VCONTROLLER_UI_BUTTONS_V2) throw std::runtime_error("unexpected config key");
		// Real loader hands readConfig a subView of just this key's payload.
		ConfigBlob payload; payload.bytes.assign(blob.bytes.begin() + 4, blob.bytes.end());
		if(!vc.readUIButtonsConfigCase(vc.app_, payload)) throw std::runtime_error("readConfig UI buttons failed");
		if(payload.size() != payload.rd) throw std::runtime_error("readConfig left " + std::to_string(payload.size() - payload.rd) + " unread bytes in key 113");
	}
	launch(vc);
	return vc;
}

// Close + reopen the app: save -> fresh process -> load -> configure.
inline VController relaunch(const VController &vc) { return loadAndConfigure(saveUIButtons(vc), vc.twoPlayer_, vc.duoLayout_); }

#ifdef HAVE_UI_DUO_EXTRA_BYTES
// Drop the PR #2 trailer from a saved key-113 blob to get the pre-PR format.
inline void stripDuoTrailer(ConfigBlob &b, size_t elems)
{
	size_t n = 1 + elems * uiButtonDuoExtraBytes;
	if(b.bytes.size() < 4 + n) throw std::runtime_error("blob too small for trailer");
	b.bytes.resize(b.bytes.size() - n);
	uint16_t sz; std::memcpy(&sz, b.bytes.data(), 2); sz = uint16_t(sz - n); std::memcpy(b.bytes.data(), &sz, 2);
}
#endif

// Old-format config: UI buttons blob written before the Duo button existed
// (default UI groups only, no toggleDuoLayout element, no duo extra fields).
inline ConfigBlob oldConfigBlobWithoutDuoButton()
{
	VController old; old.uiElements = old.defaultUIGroups();
	auto b = saveUIButtons(old);
#ifdef HAVE_UI_DUO_EXTRA_BYTES
	// This tree writes the PR #2 trailer (u8 removed + 5 bytes/element); an old build did not.
	stripDuoTrailer(b, old.uiElements.size());
#endif
	return b;
}

// Real setTwoPlayerControls / setDuoLayout (for edge cases; NOT fixed this round, see MANUAL.md).
inline void setTwoPlayer(VController &vc, bool on) { vc.setTwoPlayerControls(on); }
inline void toggleJointSeparate(VController &vc) { vc.setDuoLayout(vc.duoLayout_ == DuoLayout::Joint ? DuoLayout::Separate : DuoLayout::Joint); }
inline void resetUI(VController &vc) { vc.resetUIGroups(); }

// Default Duo button as produced by a fresh install (used as the "expected default").
struct DuoDefaults { VControllerLayoutPosition pos[2]; VControllerState state; int16_t sizeMM100x; uint8_t duoFlags; };
inline DuoDefaults defaultDuoButton()
{
	auto vc = makeDefaultController();
	auto *e = findDuoButton(vc);
	if(!e) throw std::runtime_error("fresh install has no Duo button");
	return {{e->layoutPos[0], e->layoutPos[1]}, e->state, e->sizeMM100x, e->duoFlags};
}

// ============================== harness ==============================
struct Ctx
{
	std::vector<std::string> failures;
	bool check(bool ok, std::string msg) { if(!ok) failures.push_back(std::move(msg)); return ok; }
};
struct Skip { std::string why; };
struct Case { const char *name; std::function<void(Ctx&)> fn; };

// ============================== cases ==============================

// A: remove, close/reopen -> must not come back
static void caseA_removeThenRelaunch(Ctx &t)
{
	auto vc = makeDefaultController();
	if(!t.check(countDuoButtons(vc) == 1, "precondition: fresh install should have 1 Duo button")) return;
	t.check(removeDuoButton(vc), "remove failed");
	t.check(countDuoButtons(vc) == 0, "button still present right after remove");
	auto saved = saveUIButtons(vc);
	auto vc2 = loadAndConfigure(saved);
	t.check(countDuoButtons(vc2) == 0, "after relaunch: Duo button came back (count=" + std::to_string(countDuoButtons(vc2)) + ")");
	auto vc3 = relaunch(vc2);
	t.check(countDuoButtons(vc3) == 0, "after 2nd relaunch: Duo button came back");
}

// B: remove, add back (custom place + state), close/reopen -> still there, exactly once, settings kept
static void caseB_addBackThenRelaunch(Ctx &t)
{
	auto vc = makeDefaultController();
	removeDuoButton(vc);
	vc = relaunch(vc);
	removeDuoButton(vc); // in case master re-added it
	auto &e = addDuoButtonBack(vc);
	e.layoutPos[0] = {RB2DO, {-120, -80}};
	e.layoutPos[1] = {LB2DO, {60, -200}};
	setDuoButtonState(e, VControllerState::HIDDEN);
	auto vc2 = relaunch(vc);
	t.check(countDuoButtons(vc2) == 1, "after relaunch: expected exactly 1 Duo button, got " + std::to_string(countDuoButtons(vc2)));
	auto *e2 = findDuoButton(vc2);
	if(!e2) return;
	t.check(e2->layoutPos[0].origin == RB2DO && e2->layoutPos[0].pos == VControllerLayoutPosition::Pos{-120, -80},
		"landscape position not kept (origin=" + std::to_string(e2->layoutPos[0].origin.v) + " x=" + std::to_string(e2->layoutPos[0].pos.x) + " y=" + std::to_string(e2->layoutPos[0].pos.y) + ")");
	t.check(e2->layoutPos[1].origin == LB2DO && e2->layoutPos[1].pos == VControllerLayoutPosition::Pos{60, -200}, "portrait position not kept");
	t.check(e2->state == VControllerState::HIDDEN, "state not kept (expected HIDDEN, got " + std::to_string(int(e2->state)) + ")");
}

// C: move + hide, reload -> mm position, duoFlags, sizeMM100x, state match (landscape + portrait)
static void caseC_moveHideRelaunch(Ctx &t)
{
	auto vc = makeDefaultController();
	auto *e = findDuoButton(vc);
	if(!t.check(e != nullptr, "precondition: fresh install should have a Duo button")) return;
	setDuoButtonPosMM(vc, *e, 0, RB2DO, -15.f, -12.f); // landscape
	setDuoButtonPosMM(vc, *e, 1, LC2DO, 8.f, 20.f);    // portrait
	e->sizeMM100x = 1250;                              // resized in the editor
	setDuoButtonState(*e, VControllerState::HIDDEN);
	const auto want0 = e->layoutPos[0], want1 = e->layoutPos[1];
	const auto wantFlags = e->duoFlags;
	t.check(wantFlags & VControllerElement::DUO_POS_MM, "precondition: setDuoPosition should set DUO_POS_MM");
	for(int round = 1; round <= 2; round++)
	{
		vc = relaunch(vc);
		auto tag = "after relaunch #" + std::to_string(round) + ": ";
		if(!t.check(countDuoButtons(vc) == 1, tag + "expected 1 Duo button, got " + std::to_string(countDuoButtons(vc)))) return;
		auto *e2 = findDuoButton(vc);
		t.check(e2->layoutPos[0].origin == want0.origin && e2->layoutPos[0].pos == want0.pos,
			tag + "landscape mm position not kept (origin=" + std::to_string(e2->layoutPos[0].origin.v) + " x=" + std::to_string(e2->layoutPos[0].pos.x) + " y=" + std::to_string(e2->layoutPos[0].pos.y) + ")");
		t.check(e2->layoutPos[1].origin == want1.origin && e2->layoutPos[1].pos == want1.pos, tag + "portrait mm position not kept");
		t.check(e2->duoFlags & VControllerElement::DUO_POS_MM, tag + "DUO_POS_MM lost: mm*10 position would be read as pixels");
		t.check(e2->duoFlags == wantFlags, tag + "duoFlags changed (" + std::to_string(wantFlags) + " -> " + std::to_string(e2->duoFlags) + ")");
		t.check(e2->sizeMM100x == 1250, tag + "sizeMM100x not kept (got " + std::to_string(e2->sizeMM100x) + ")");
		t.check(e2->state == VControllerState::HIDDEN, tag + "state not kept (expected HIDDEN, got " + std::to_string(int(e2->state)) + ")");
	}
}

// D: Reset UI Groups restores the button with defaults
static void caseD_resetUIGroups(Ctx &t)
{
	auto def = defaultDuoButton();
	auto vc = makeDefaultController();
	removeDuoButton(vc);
	vc = relaunch(vc);
	resetUI(vc);
	t.check(countDuoButtons(vc) == 1, "after Reset UI Groups: expected 1 Duo button, got " + std::to_string(countDuoButtons(vc)));
	auto *e = findDuoButton(vc);
	if(!e) return;
	t.check(e->state == VControllerState::SHOWN, "after reset: state not SHOWN");
	t.check(e->layoutPos[0].origin == def.pos[0].origin && e->layoutPos[0].pos == def.pos[0].pos
		&& e->layoutPos[1].origin == def.pos[1].origin && e->layoutPos[1].pos == def.pos[1].pos, "after reset: position not default");
	t.check(e->duoFlags == def.duoFlags && e->sizeMM100x == def.sizeMM100x, "after reset: duoFlags/size not default");
	auto vc2 = relaunch(vc);
	t.check(countDuoButtons(vc2) == 1, "after reset + relaunch: expected 1 Duo button, got " + std::to_string(countDuoButtons(vc2)));
}

// E: old config without this button -> button shown with defaults
static void caseE_oldConfigWithoutButton(Ctx &t)
{
	auto def = defaultDuoButton();
	auto checkDefault = [&](VController &vc, std::string tag)
	{
		if(!t.check(countDuoButtons(vc) == 1, tag + "expected 1 Duo button, got " + std::to_string(countDuoButtons(vc)))) return;
		auto *e = findDuoButton(vc);
		t.check(e->state == VControllerState::SHOWN, tag + "state not SHOWN");
		t.check(e->layoutPos[0].origin == def.pos[0].origin && e->layoutPos[0].pos == def.pos[0].pos
			&& e->layoutPos[1].origin == def.pos[1].origin && e->layoutPos[1].pos == def.pos[1].pos, tag + "position not default");
		t.check(e->duoFlags == def.duoFlags && e->sizeMM100x == def.sizeMM100x, tag + "duoFlags/size not default");
	};
	// E1: key 113 present, written by a build before the Duo button existed
	auto vc = loadAndConfigure(oldConfigBlobWithoutDuoButton());
	checkDefault(vc, "old key 113 without button: ");
	t.check(countDuoButtons(relaunch(vc)) == 1, "old key 113 without button: button gone after a 2nd relaunch");
	// E2: no key 113 at all
	auto vc2 = loadAndConfigure(ConfigBlob{});
	checkDefault(vc2, "no key 113: ");
}

// F: leaving the layout editor writes config immediately.
// Seam (PR #2): views' onDismiss() -> REAL VController::onLayoutEditorClosed() -> app().saveConfigFile().
// The GUI views themselves can't be built on host, so their wiring is checked as source probes
// (extract.py), and the seam's effect is checked by "killing" the app right after it runs:
// the only data that survives is what saveConfigFile wrote.
static void caseF_saveOnLeavingEditor(Ctx &t)
{
#ifndef HAVE_ON_LAYOUT_EDITOR_CLOSED
	(void)t;
	throw Skip{"no onLayoutEditorClosed() seam in this tree; manual step F in MANUAL.md"};
#else
#ifndef HAVE_VIRTUAL_ON_DISMISS
	t.check(false, "View::onDismiss() is not virtual: editor views' onDismiss overrides would never run");
#endif
#ifndef HAVE_TOUCHCONFIG_DISMISS_SAVES
	t.check(false, "TouchConfigView::onDismiss() does not call vController.onLayoutEditorClosed()");
#endif
#ifndef HAVE_PLACEVCONTROLS_DISMISS_SAVES
	t.check(false, "PlaceVControlsView::onDismiss() does not call vController.onLayoutEditorClosed()");
#endif
	ConfigBlob disk; // the config file on "disk"
	auto vc = makeDefaultController();
	vc.app_.onSaveConfigFile = [&]{ disk = saveUIButtons(vc); };
	// edit 1: remove Mode button, leave editor, kill
	removeDuoButton(vc);
	vc.onLayoutEditorClosed();
	t.check(vc.app_.saveConfigFileCalls == 1, "leaving editor: saveConfigFile called " + std::to_string(vc.app_.saveConfigFileCalls) + " times, expected 1");
	if(!t.check(!disk.bytes.empty(), "leaving editor: nothing written to config")) return;
	auto killed = loadAndConfigure(disk);
	t.check(countDuoButtons(killed) == 0, "kill right after leaving editor: removal lost, Duo button came back");
	// edit 2: Reset UI Groups (button back, moved+hidden), leave editor, kill
	resetUI(vc);
	auto *e = findDuoButton(vc);
	if(!t.check(e != nullptr, "reset: no Duo button")) return;
	setDuoButtonPosMM(vc, *e, 0, RB2DO, -11.f, -9.f);
	setDuoButtonState(*e, VControllerState::HIDDEN);
	auto want = e->layoutPos[0];
	vc.onLayoutEditorClosed();
	auto killed2 = loadAndConfigure(disk);
	auto *e2 = findDuoButton(killed2);
	if(!t.check(e2 != nullptr, "kill after 2nd edit: Duo button missing")) return;
	t.check(e2->layoutPos[0].origin == want.origin && e2->layoutPos[0].pos == want.pos && e2->state == VControllerState::HIDDEN,
		"kill after 2nd edit: moved/hidden state not in the saved config");
#endif
}

// G (edge, in main scope): remove -> switch Joint/Separate -> relaunch -> stays removed
static void caseG_removeToggleLayoutRelaunch(Ctx &t)
{
	auto vc = makeDefaultController();
	removeDuoButton(vc);
	toggleJointSeparate(vc);
	t.check(countDuoButtons(vc) == 0, "right after Joint/Separate switch: Duo button came back while running");
	auto layout = vc.duoLayout_;
	auto vc2 = relaunch(vc);
	t.check(vc2.duoLayout_ == layout, "duoLayout_ changed across relaunch");
	t.check(countDuoButtons(vc2) == 0, "after switch + relaunch: Duo button came back (count=" + std::to_string(countDuoButtons(vc2)) + ")");
	toggleJointSeparate(vc2);
	auto vc3 = relaunch(vc2);
	t.check(countDuoButtons(vc3) == 0, "after switching back + relaunch: Duo button came back");
}

// H (edge, in main scope): user removes every UI button -> save -> relaunch must not crash
// (configure() reads uiElements[0] for the v1 sentinel; empty list must be safe)
static void caseH_allUIButtonsRemoved(Ctx &t)
{
	auto vc = makeDefaultController();
	removeAllUIButtons(vc); // real remove() path, as the user would
	if(!t.check(vc.uiElements.empty(), "removeAllUIButtons left elements")) return;
	auto vc2 = relaunch(vc); // an exception/crash here fails the case
	t.check(countDuoButtons(vc2) == 0, "after removing all UI buttons + relaunch: Duo button came back");
	t.check(vc2.uiElements.empty(), "after removing all UI buttons + relaunch: " + std::to_string(vc2.uiElements.size()) + " UI elements reappeared");
	auto vc3 = relaunch(vc2);
	t.check(vc3.uiElements.empty(), "after 2nd relaunch: UI elements reappeared");
}

static const std::vector<Case> CASES
{
	{"A remove -> relaunch -> stays removed", caseA_removeThenRelaunch},
	{"B add back -> relaunch -> kept (pos/state)", caseB_addBackThenRelaunch},
	{"C move/hide -> relaunch -> mm pos/flags/size/state", caseC_moveHideRelaunch},
	{"D Reset UI Groups -> button restored", caseD_resetUIGroups},
	{"E old config without button -> default button", caseE_oldConfigWithoutButton},
	{"F leave layout editor -> config saved now", caseF_saveOnLeavingEditor},
	{"G remove -> Joint/Separate -> relaunch -> stays removed", caseG_removeToggleLayoutRelaunch},
	{"H remove all UI buttons -> relaunch -> no crash", caseH_allUIButtonsRemoved},
	// edge cases go here
};

}

int main(int argc, char **argv)
{
	using namespace EmuEx;
	int pass = 0, fail = 0, skip = 0;
	for(auto &c : CASES)
	{
		if(argc > 1 && std::string(c.name).find(argv[1]) == std::string::npos) continue;
		Ctx t;
		try { c.fn(t); }
		catch(Skip &s) { std::printf("SKIP  %s  (%s)\n", c.name, s.why.c_str()); skip++; continue; }
		catch(std::exception &e) { t.failures.push_back(std::string("exception: ") + e.what()); }
		if(t.failures.empty()) { std::printf("PASS  %s\n", c.name); pass++; }
		else
		{
			std::printf("FAIL  %s\n", c.name); fail++;
			for(auto &f : t.failures) std::printf("        - %s\n", f.c_str());
		}
	}
	std::printf("\n%d passed, %d failed, %d skipped\n", pass, fail, skip);
	return fail ? 1 : 0;
}
