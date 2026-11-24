// Saturn-Quad 4-player touch layout: shared host-test harness API.
// Case files in cases/*.cc include ONLY this header and use the helpers + macros below.
// The backend (core/backend_*.cc) decides what code actually runs:
//   real  : extracted REAL EmuFramework/Saturn.emu code (needs the 4-player implementation; else PENDING)
//   model : reference model of the SPEC (QUAD_BACKEND=model) -- validates the test logic only,
//           results are labelled MODEL and never count as a pass of the implementation.
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace quad {

constexpr int numSets = 4;           // pad sets / players / duoRole 0..3
constexpr int uiSet = -1;            // shared UI buttons (Open Menu, FF/Rewind, Mode)
constexpr uint8_t removedFlag = 0x80; // duoFlags bit 7 (NEO-Duo f81076b)

// Button ids (symbolic; backends map to AppKeyCode / Saturn key codes)
enum Key : int {
	// per-set game buttons (Saturn pad)
	Dpad = 1, A, B, C, X, Y, Z, L, R, Start,
	// shared UI buttons
	OpenMenu = 100, FastForward, Rewind, Mode /* three-line Toggle Control Layout (toggleDuoLayout) */,
};
const char *keyName(int k);
std::vector<int> gameKeys();          // Dpad..Start
std::vector<int> defaultUIKeys();     // OpenMenu, FastForward, Rewind, Mode (real defaultUIGroups + Mode)

struct Pos { int x = 0, y = 0; bool operator==(const Pos &) const = default; };
using PortInputs = std::array<uint32_t, numSets>; // input bits seen on Saturn ports 1..4 (index 0..3)
struct Rgb { uint8_t r, g, b; bool operator==(const Rgb &) const = default; };

// ---- helpers (all act on the current in-memory app instance) ----
void freshInstall();                  // default 4-set layout from real defaults (A2/G2 style, NOT Mode-only)
void freshInstallModeOnlyUI();        // legacy NEO-Duo A..H start (UI = Mode only)
std::vector<uint8_t> save();          // write config (key 113 UI_BUTTONS_V2 + game buttons), returns bytes
void relaunch();                      // save -> destroy -> new instance -> load saved bytes -> configure
void relaunch(int times);
void loadConfig(const std::vector<uint8_t> &bytes); // fresh instance loading given bytes
void openLayoutEditor();
void closeLayoutEditor();             // must persist (onLayoutEditorClosed -> saveConfigFile)
void killAfterEditor();               // simulate process kill right after closing editor; reload what was saved

bool exists(int set, int key);        // button present (not removed)
bool hidden(int set, int key);
Pos pos(int set, int key);
int count(int set);                   // buttons present in set (uiSet for UI)
void removeBtn(int set, int key);     // TouchConfigView remove (real VController::remove)
void removeAllUI();
void addBack(int set, int key);       // TouchConfigView add (noteUIButtonAdded)
void moveBtn(int set, int key, Pos p);
void hideBtn(int set, int key, bool hide);
PortInputs press(int set, int key);   // touch the button of set N -> what each Saturn port receives
Rgb color(int set);
void switchLayout();                  // Mode button action: Joint/Separate (or next layout)
void setPlayers(int n);               // 1,2,4 player touch layouts
int players();
void setInputPlayer(int player);      // Virtual Gamepad Player, 0-based (1P port)
int inputPlayer();
uint8_t savedDuoFlags(const std::vector<uint8_t> &cfg, int set, int key); // from BYTES, not memory
uint8_t savedDuoRole(const std::vector<uint8_t> &cfg, int set, int key);
std::vector<uint8_t> neoDuo2PlayerConfig(); // config blob in NEO-Duo v1.1 format (roles 0..1)
void resetUIGroups();                 // "Reset UI Groups": restores default UI incl. Mode, clears removed flag
bool multitapEnabled(int port);       // Saturn SMPC multitap on port 0/1 (input.cc SMPC_SetMultitap)
bool modeHoldShouldToggle(bool pressBeganOnMode, bool heldLongEnough);
bool modePressRemainsPending(bool pending, bool samePointer, bool releasedOrCanceled);
bool midGameOverlayTap(bool currentlyOn, bool wanted, bool gameLoaded, bool singlePlayer = false);
bool midGameOverlayDeferred(bool currentlyOn, bool wanted, bool gameLoaded, bool singlePlayer = false);
int overlayForcedPadEnd(int port, int playerCount, int inputPlayer);
void setPadSize(int set, int sizeMM100x); int padSize(int set);   // per-player size (P3/P4 must not overwrite P2)
void setPadAlpha(int set, int alpha); int padAlpha(int set);
PortInputs pressTurbo(int set, int key); // shared turbo/toggle path (EmuInput.cc) for a set's button
constexpr uint8_t playerSetShift = 3, playerSetMask = 0x38; // duoFlags bits 3-5 = playerSet (must not touch bit 7)
// Lead decision 21:44: accept PR #4 encoding (QuadControls.hh setFieldForPlayer): P1..P4 -> 0,1,3,4 (2 = legacy shared row)
constexpr uint8_t playerSetField(int s) { constexpr uint8_t v[4]{0, 1, 3, 4}; return s >= 0 && s < 4 ? v[s] : 0; }

// ---- results ----
enum class Result { Pass, Fail, Skip, Pending, XFail, XPass };
struct Case { std::string name; std::function<void()> fn; bool knownIssue; };
std::vector<Case> &registry();
struct Reg { Reg(const char *n, std::function<void()> f, bool ki = false) { registry().push_back({n, f, ki}); } };
struct Abort { Result r; std::string why; };
void check(bool ok, const char *expr, const char *file, int line, const std::string &msg = {});
bool backendReady(std::string &why);  // false -> every case PENDING with why
const char *backendName();

} // namespace quad

#define QUAD_CAT2(a,b) a##b
#define QUAD_CAT(a,b) QUAD_CAT2(a,b)
// CASE("p1 C remove Mode persists") { ...body... }
#define CASE(name) static void QUAD_CAT(case_fn_, __LINE__)(); \
	static quad::Reg QUAD_CAT(case_reg_, __LINE__)(name, QUAD_CAT(case_fn_, __LINE__)); \
	static void QUAD_CAT(case_fn_, __LINE__)()
// Known issue: failure -> XFAIL, success -> XPASS; never changes exit code.
#define XFAIL_CASE(name) static void QUAD_CAT(case_fn_, __LINE__)(); \
	static quad::Reg QUAD_CAT(case_reg_, __LINE__)(name, QUAD_CAT(case_fn_, __LINE__), true); \
	static void QUAD_CAT(case_fn_, __LINE__)()
#define CHECK(cond) quad::check((cond), #cond, __FILE__, __LINE__)
#define CHECK_MSG(cond, msg) quad::check((cond), #cond, __FILE__, __LINE__, (msg))
#define PENDING(why) throw quad::Abort{quad::Result::Pending, (why)}
#define SKIP(why) throw quad::Abort{quad::Result::Skip, (why)}
// for every set: FOR_SETS(s) { ... }
#define FOR_SETS(s) for(int s = 0; s < quad::numSets; ++s)
