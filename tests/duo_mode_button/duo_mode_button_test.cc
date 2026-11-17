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

// "Remove This Button" in TouchConfigView (erases the element from uiElements).
inline bool removeDuoButton(VController &vc)
{
	return std::erase_if(vc.uiElements, isDuoModeButton) > 0;
}

// "Add Button" in TouchConfigView: VController::add(InputComponentDesc{ui, {toggleDuoLayout}, CT2DO}).
// SIMULATED: real add() places it at the layout centre in pixels.
inline VControllerElement &addDuoButtonBack(VController &vc)
{
	auto &e = vc.uiElements.emplace_back(std::in_place_type<VControllerUIButtonGroup>,
		VControllerUIButtonGroup::Config{{KeyInfo::appKey(AppKeyCode::toggleDuoLayout)}, {1, CT2DO}});
	e.layoutPos[0] = e.layoutPos[1] = {C2DO, {0, 0}};
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
		if(!vc.readUIButtonsConfigCase(vc.app_, blob)) throw std::runtime_error("readConfig UI buttons failed");
	}
	launch(vc);
	return vc;
}

// Close + reopen the app: save -> fresh process -> load -> configure.
inline VController relaunch(const VController &vc) { return loadAndConfigure(saveUIButtons(vc), vc.twoPlayer_, vc.duoLayout_); }

// Old-format config: UI buttons blob written before the Duo button existed
// (default UI groups only, no toggleDuoLayout element, no duo extra fields).
inline ConfigBlob oldConfigBlobWithoutDuoButton()
{
	VController old; old.uiElements = old.defaultUIGroups();
	return saveUIButtons(old);
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
static void caseC_moveHideRelaunch(Ctx &)
{
	// TODO((ตก) 4): use setDuoButtonPosMM(vc, *e, 0/1, ...), setDuoButtonState(*e, HIDDEN),
	// relaunch(vc), then compare layoutPos[0]/[1], duoFlags & DUO_POS_MM, sizeMM100x, state.
	throw Skip{"TODO: owned by (ตก) 4"};
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
static void caseE_oldConfigWithoutButton(Ctx &)
{
	// TODO((ตก) 4): auto vc = loadAndConfigure(oldConfigBlobWithoutDuoButton()); compare with defaultDuoButton().
	throw Skip{"TODO: owned by (ตก) 4"};
}

// F: leaving the layout editor writes config immediately
static void caseF_saveOnLeavingEditor(Ctx &)
{
	// TouchConfigView is a full GUI View (needs ViewAttachParams, window, renderer);
	// not host-testable without a seam. See MANUAL.md case F.
	throw Skip{"not host-testable: TouchConfigView needs GUI; manual step F in MANUAL.md"};
}

static const std::vector<Case> CASES
{
	{"A remove -> relaunch -> stays removed", caseA_removeThenRelaunch},
	{"B add back -> relaunch -> kept (pos/state)", caseB_addBackThenRelaunch},
	{"C move/hide -> relaunch -> mm pos/flags/size/state", caseC_moveHideRelaunch},
	{"D Reset UI Groups -> button restored", caseD_resetUIGroups},
	{"E old config without button -> default button", caseE_oldConfigWithoutButton},
	{"F leave layout editor -> config saved now", caseF_saveOnLeavingEditor},
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
