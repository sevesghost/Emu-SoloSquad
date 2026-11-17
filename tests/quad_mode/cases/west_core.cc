// west (deputy) core cases: the NEO-Duo A..I / A2 / G2 set generalized to 4 pad sets, registered
// once per set ("P1".."P4"), plus the button-bug gates the user asked for. Staff files add more.
#include "quad_harness.hh"
#include <string>
using namespace quad;
namespace {
std::string S(int s) { return "set P" + std::to_string(s + 1); }
// snapshot of one set, used to prove other sets are untouched
struct Snap { bool ex[16]{}, hid[16]{}; Pos p[16]; };
Snap snap(int s) { Snap n; for(int k : gameKeys()) { n.ex[k] = exists(s, k); n.hid[k] = hidden(s, k); if(n.ex[k]) n.p[k] = pos(s, k); } return n; }
void sameAs(int s, const Snap &a, const char *when) {
	for(int k : gameKeys()) {
		CHECK_MSG(exists(s, k) == a.ex[k], S(s) + " " + keyName(k) + " presence changed " + when);
		CHECK_MSG(hidden(s, k) == a.hid[k], S(s) + " " + keyName(k) + " hidden changed " + when);
		if(a.ex[k]) CHECK_MSG(pos(s, k) == a.p[k], S(s) + " " + keyName(k) + " moved " + when);
	}
}
void modeGoneAndCarried(const char *when) {
	CHECK_MSG(!exists(uiSet, Mode), std::string("Mode button came back ") + when);
	CHECK_MSG(savedDuoFlags(save(), uiSet, OpenMenu) & removedFlag, std::string("bit7 not in saved bytes ") + when);
}
using Fn = void (*)(int);
struct PerSet { PerSet(const char *id, Fn f, bool ki = false) {
	for(int s = 0; s < numSets; ++s) { std::string n = std::string(id) + " [P" + std::to_string(s + 1) + "]";
		registry().push_back({n, [f, s] { f(s); }, ki}); } } };

// A: routing -- every game button of set s reaches only port s
PerSet case_A("A route: each button of set -> own port only", [](int s) {
	freshInstall(); setPlayers(4);
	for(int k : gameKeys()) { auto in = press(s, k);
		for(int p = 0; p < numSets; ++p) CHECK_MSG((in[p] != 0) == (p == s), S(s) + " " + keyName(k) + " seen on port " + std::to_string(p + 1)); }
});
// A2 (NEO-Duo A2): real default UI set, delete Mode, editor close saves, relaunch x2, bit7 carried
PerSet case_A2("A2 delete Mode from real defaults, relaunch x2", [](int s) {
	freshInstall(); auto before = snap(s);
	openLayoutEditor(); removeBtn(uiSet, Mode); closeLayoutEditor();
	relaunch(); modeGoneAndCarried("after relaunch 1"); relaunch(); modeGoneAndCarried("after relaunch 2");
	CHECK(exists(uiSet, OpenMenu) && exists(uiSet, FastForward) && exists(uiSet, Rewind));
	sameAs(s, before, "after Mode delete");
});
// B: distinct colors
PerSet case_B("B color distinct from other sets", [](int s) {
	freshInstall(); for(int o = 0; o < numSets; ++o) if(o != s) CHECK_MSG(!(color(o) == color(s)), S(s) + " same color as P" + std::to_string(o + 1));
});
// C: delete a game button of set s, relaunch x2, stays deleted; others untouched; re-add works
PerSet case_C("C delete button persists x2 relaunch, isolated, re-add", [](int s) {
	freshInstall(); Snap other[numSets]; for(int o = 0; o < numSets; ++o) other[o] = snap(o);
	openLayoutEditor(); removeBtn(s, B); removeBtn(s, Start); closeLayoutEditor();
	relaunch(2);
	CHECK_MSG(!exists(s, B) && !exists(s, Start), S(s) + " deleted button came back");
	CHECK(press(s, B)[s] == 0);
	for(int o = 0; o < numSets; ++o) if(o != s) sameAs(o, other[o], "after deleting in another set");
	addBack(s, B); closeLayoutEditor(); relaunch();
	CHECK_MSG(exists(s, B), S(s) + " re-added button lost"); CHECK(!exists(s, Start));
});
// C2: move persists
PerSet case_Cm("C move button persists x2 relaunch, isolated", [](int s) {
	freshInstall(); Snap other[numSets]; for(int o = 0; o < numSets; ++o) other[o] = snap(o);
	Pos np{123 + s * 7, 456 + s * 3}; moveBtn(s, A, np); closeLayoutEditor(); relaunch(2);
	CHECK_MSG(pos(s, A) == np, S(s) + " moved A not persisted");
	for(int o = 0; o < numSets; ++o) if(o != s) sameAs(o, other[o], "after moving in another set");
});
// C3: hide persists and a hidden button sends nothing
PerSet case_Ch("C hide button persists x2 relaunch, isolated, unhide", [](int s) {
	freshInstall(); Snap other[numSets]; for(int o = 0; o < numSets; ++o) other[o] = snap(o);
	hideBtn(s, X, true); closeLayoutEditor(); relaunch(2);
	CHECK_MSG(hidden(s, X), S(s) + " hidden X shown again"); CHECK(exists(s, X));
	for(int o = 0; o < numSets; ++o) if(o != s) sameAs(o, other[o], "after hiding in another set");
	hideBtn(s, X, false); closeLayoutEditor(); relaunch(); CHECK(!hidden(s, X));
});
// D: bit7 survives layout switches + two relaunches
PerSet case_D("D Mode removed survives layout switch + 2 relaunches", [](int s) {
	freshInstall(); auto before = snap(s); removeBtn(uiSet, Mode); closeLayoutEditor();
	switchLayout(); relaunch(); switchLayout(); relaunch(); switchLayout();
	modeGoneAndCarried("after switches"); sameAs(s, before, "after layout switches");
	CHECK_MSG(count(s) == (int)gameKeys().size(), S(s) + " buttons duplicated/lost by layout switch");
});
// E: old NEO-Duo 2P config loads; roles 2..3 default
PerSet case_E("E NEO-Duo 2-player config loads, sets 3-4 default", [](int s) {
	loadConfig(neoDuo2PlayerConfig());
	// Lead decision 21:49: a config with no player-count block stays 2P (no auto-4P / multitap)
	CHECK_MSG(players() == 2, "old 2P config (no player block) did not load as 2P: players()=" + std::to_string(players()));
	CHECK_MSG(!multitapEnabled(0), "old 2P config enabled port-1 multitap");
	CHECK_MSG(count(s) == (int)gameKeys().size(), S(s) + " wrong button count from old config");
	auto cfg = save(); CHECK_MSG(savedDuoRole(cfg, s, A) == s, S(s) + " duoRole not " + std::to_string(s));
	if(s >= 2) { CHECK_MSG(press(s, A)[s] == 0, S(s) + " button active while old config is 2P"); setPlayers(4); }
	CHECK(press(s, A)[s] != 0);
});
// F: original NEO-Duo bug -- delete Mode, process killed right after editor closes
PerSet case_F("F delete Mode, kill after editor close, stays deleted", [](int s) {
	freshInstall(); auto before = snap(s);
	openLayoutEditor(); removeBtn(uiSet, Mode); closeLayoutEditor(); killAfterEditor();
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back after kill (editor close did not save)");
	relaunch(); CHECK(!exists(uiSet, Mode)); sameAs(s, before, "after Mode delete+kill");
});
// G / G2: delete Mode then switch layout before saving (NEO-Duo G2 = real defaults)
PerSet case_G2("G2 delete Mode + layout switch before save, real defaults", [](int s) {
	freshInstall(); removeBtn(uiSet, Mode); switchLayout(); closeLayoutEditor(); relaunch(2);
	modeGoneAndCarried("G2"); CHECK(count(s) == (int)gameKeys().size());
});
// G: empty UI layout placeholder -- delete ALL UI buttons, relaunch x2, nothing recreated
PerSet case_G("G delete all UI buttons, relaunch x2, none recreated", [](int s) {
	freshInstall(); auto before = snap(s); removeAllUI(); closeLayoutEditor(); relaunch(2);
	CHECK_MSG(count(uiSet) == 0, "UI buttons recreated after deleting all: " + std::to_string(count(uiSet)));
	CHECK(savedDuoFlags(save(), uiSet, Mode) & removedFlag); sameAs(s, before, "after deleting all UI");
});
// H: legacy Mode-only start (NEO-Duo A..H path)
PerSet case_H("H Mode-only UI start, delete Mode, relaunch x2", [](int s) {
	freshInstallModeOnlyUI(); removeBtn(uiSet, Mode); closeLayoutEditor(); relaunch(2);
	CHECK(!exists(uiSet, Mode)); CHECK(count(s) == (int)gameKeys().size());
});
// I: known issue from NEO-Duo -- delete all UI, relaunch, add back non-Mode one by one
PerSet case_I("I delete all UI, add back non-Mode one at a time -> Mode stays gone", [](int s) {
	freshInstall(); removeAllUI(); closeLayoutEditor(); relaunch();
	for(int k : {OpenMenu, FastForward, Rewind}) { addBack(uiSet, k); closeLayoutEditor(); relaunch();
		CHECK_MSG(!exists(uiSet, Mode), std::string("Mode reappeared after re-adding ") + keyName(k)); }
	CHECK(count(s) == (int)gameKeys().size());
});  // was XFAIL in NEO-Duo; north fixes it this round -> required
// ---- south code-analysis targets (S1..S6) ----
PerSet case_S2("S2 per-player size/alpha persist, P3/P4 do not overwrite P2", [](int s) {
	freshInstall(); FOR_SETS(o) { setPadSize(o, 900 + o * 50); setPadAlpha(o, 100 + o * 10); }
	closeLayoutEditor(); relaunch(2);
	FOR_SETS(o) { CHECK_MSG(padSize(o) == 900 + o * 50, "P" + std::to_string(o + 1) + " size lost/overwritten");
		CHECK_MSG(padAlpha(o) == 100 + o * 10, "P" + std::to_string(o + 1) + " alpha lost/overwritten"); }
	(void)s;
});
PerSet case_S3("S3 turbo/toggle shared path reaches own port", [](int s) {
	freshInstall(); setPlayers(4); auto in = pressTurbo(s, A);
	for(int p = 0; p < numSets; ++p) CHECK_MSG((in[p] != 0) == (p == s), S(s) + " turbo A seen on port " + std::to_string(p + 1));
});
PerSet case_S4("S4 playerSet bits 3-5 == setFieldForPlayer(role) (0,1,3,4), bit7 untouched", [](int s) {
	freshInstall(); auto cfg = save(); uint8_t f = savedDuoFlags(cfg, s, A);
	CHECK_MSG(((f & playerSetMask) >> playerSetShift) == playerSetField(s), S(s) + " playerSet bits wrong");
	CHECK_MSG(!(f & removedFlag), S(s) + " bit7 set on a present button");
	removeBtn(uiSet, Mode); cfg = save(); f = savedDuoFlags(cfg, s, A);
	CHECK_MSG(((f & playerSetMask) >> playerSetShift) == playerSetField(s), S(s) + " playerSet clobbered by Mode removal");
});
PerSet case_S5("S5 player-count switch 4/2/1/4 does not resurrect Mode", [](int s) {
	freshInstall(); removeBtn(uiSet, Mode); closeLayoutEditor();
	for(int n : {2, 1, 4, 2, 4}) { setPlayers(n); CHECK_MSG(!exists(uiSet, Mode), "Mode back after setPlayers(" + std::to_string(n) + ")"); }
	relaunch(2); modeGoneAndCarried("after player-count switches"); CHECK(count(s) == (int)gameKeys().size());
});
PerSet case_D2("D2 Reset UI Groups restores Mode and clears removed flag", [](int s) {
	freshInstall(); removeBtn(uiSet, Mode); closeLayoutEditor(); relaunch();
	resetUIGroups(); closeLayoutEditor(); relaunch(2);
	CHECK(exists(uiSet, Mode)); CHECK(!(savedDuoFlags(save(), uiSet, OpenMenu) & removedFlag)); CHECK(count(s) == (int)gameKeys().size());
});
PerSet case_C4("C4 delete ALL buttons of set, relaunch x2, not refilled; others intact", [](int s) {
	freshInstall(); Snap other[numSets]; for(int o = 0; o < numSets; ++o) other[o] = snap(o);
	for(int k : gameKeys()) removeBtn(s, k); closeLayoutEditor(); relaunch(2);
	CHECK_MSG(count(s) == 0, S(s) + " refilled with defaults after user removed all (" + std::to_string(count(s)) + ")");
	for(int o = 0; o < numSets; ++o) if(o != s) sameAs(o, other[o], "after another set removed all");
	addBack(s, Start); closeLayoutEditor(); relaunch(); CHECK(exists(s, Start) && count(s) == 1);
});
CASE("S1 4-player mode enables Saturn port-1 multitap; 2P does not need it") {
	freshInstall(); setPlayers(4); CHECK_MSG(multitapEnabled(0), "4P without port-1 multitap: P3/P4 have no port");
	relaunch(); CHECK(multitapEnabled(0));
	setPlayers(2); CHECK(!multitapEnabled(0));
}
CASE("S1b 4P: open game -> resetSessionOptions()/applyInputConfig keeps multitaps[0]=true, devs[0..3]=gamepad") {
	PENDING("Saturn.emu session path (EmuSystem.cc:352 resetSessionOptions, EmuApp.cc:1041 applyInputConfig, input.cc:101-123) needs Mednafen SMPC + EmuApp; not host-testable in this harness -- verify on device / via ออก code review");
}
CASE("S6 remove all UI then sync (relaunch/player switch) does not recreate Mode") {
	freshInstall(); removeAllUI(); closeLayoutEditor();
	setPlayers(2); setPlayers(4); relaunch(); switchLayout(); relaunch();
	CHECK(count(uiSet) == 0); CHECK(!exists(uiSet, Mode));
}
CASE("O1 saved key-113 size consistent, no bytes left after save/load/save") {
	freshInstall(); removeBtn(2, B); moveBtn(3, A, {7, 7}); hideBtn(1, X, true); removeBtn(uiSet, Mode);
	auto a = save(); loadConfig(a); auto b = save();
	CHECK_MSG(a == b, "save -> load -> save not byte-identical");
}
}
