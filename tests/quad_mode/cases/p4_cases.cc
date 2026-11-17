// P4 (player index 3, duoRole 3, Saturn port 4) cases. Owner: west-4.
// Include-only header, official helpers, names "p4 " + NEO-Duo letter (README).
// Start from freshInstall() (real default UI: OpenMenu, FastForward+Rewind, Mode),
// not the Mode-only layout. No faked passes: the real backend marks every case
// PENDING until the 4P code is in the tree. Model results are labelled MODEL.
// P4-specific angles: role 3 does not exist in NEO-Duo 2P configs (E), and
// P4 is the last set, so off-by-one / "numSets-1" bugs land here first.
#include "quad_harness.hh"
using namespace quad;

constexpr int p4 = 3; // P4

static void p4Start() {
	freshInstall();
	setPlayers(4);
	CHECK(players() == 4);
	for(int k : defaultUIKeys())
		CHECK_MSG(exists(uiSet, k), std::string("missing default UI ") + keyName(k));
	FOR_SETS(s)
		for(int k : gameKeys())
			CHECK_MSG(exists(s, k), std::string("missing ") + keyName(k) + " on set " + std::to_string(s));
}

struct P4Snap { bool ex[16]{}, hid[16]{}; Pos p[16]; };
static P4Snap p4Snap(int s) {
	P4Snap n{};
	for(int k : gameKeys()) {
		n.ex[k] = exists(s, k);
		n.hid[k] = hidden(s, k);
		if(n.ex[k]) n.p[k] = pos(s, k);
	}
	return n;
}
static void p4Same(int s, const P4Snap &a, const char *when) {
	for(int k : gameKeys()) {
		auto who = std::string("P") + std::to_string(s + 1) + " " + keyName(k);
		CHECK_MSG(exists(s, k) == a.ex[k], who + " presence changed " + when);
		CHECK_MSG(hidden(s, k) == a.hid[k], who + " hidden changed " + when);
		if(a.ex[k]) CHECK_MSG(pos(s, k) == a.p[k], who + " moved " + when);
	}
}
static void p4SnapAll(P4Snap out[numSets]) { FOR_SETS(s) out[s] = p4Snap(s); }
static void p4AllSame(const P4Snap all[numSets], const char *when, int exceptSet = -2) {
	FOR_SETS(s) if(s != exceptSet) p4Same(s, all[s], when);
}
static void p4UIRest() {
	CHECK(exists(uiSet, OpenMenu));
	CHECK(exists(uiSet, FastForward));
	CHECK(exists(uiSet, Rewind));
}
static void p4Bit7(const char *when) {
	auto cfg = save();
	for(int k : {OpenMenu, FastForward, Rewind})
		CHECK_MSG(savedDuoFlags(cfg, uiSet, k) & removedFlag,
			std::string("bit 7 (MODE_REMOVED) missing on saved ") + keyName(k) + " " + when);
}
static void p4RolesSaved(const char *when) {
	auto cfg = save();
	for(int k : gameKeys())
		if(exists(p4, k))
			CHECK_MSG(savedDuoRole(cfg, p4, k) == 3,
				std::string("P4 ") + keyName(k) + " saved with role != 3 " + when);
}

// A: remove shared Mode, relaunch x2 -> stays gone; all 4 pad sets untouched
CASE("p4 A remove Mode, relaunch x2, stays gone, P4 pads intact") {
	p4Start();
	P4Snap all[numSets]; p4SnapAll(all);
	removeBtn(uiSet, Mode);
	for(int i = 1; i <= 2; ++i) {
		relaunch();
		CHECK_MSG(!exists(uiSet, Mode), "Mode came back after relaunch " + std::to_string(i));
		p4UIRest();
		p4AllSame(all, "after Mode removal + relaunch");
	}
}

// A-pad: remove P4 Start (last set, last key), relaunch x2 -> stays gone, others intact
CASE("p4 A-pad remove P4 Start, relaunch x2, stays gone, others intact") {
	p4Start();
	P4Snap all[numSets]; p4SnapAll(all);
	removeBtn(p4, Start);
	relaunch(2);
	CHECK_MSG(!exists(p4, Start), "P4 Start came back");
	CHECK_MSG(count(p4) == int(gameKeys().size()) - 1, "P4 button count wrong after removing Start");
	p4AllSame(all, "after P4 Start removal", p4);
	for(int k : gameKeys()) if(k != Start) CHECK(exists(p4, k));
	for(int k : defaultUIKeys()) CHECK(exists(uiSet, k));
}

// A2: default UI start, remove Mode, relaunch x2, bit 7 on every remaining UI group in BYTES
CASE("p4 A2 default UI: remove Mode, relaunch x2, bit7 on remaining UI, P4 intact") {
	p4Start();
	auto before = p4Snap(p4);
	removeBtn(uiSet, Mode);
	p4Bit7("right after remove");
	for(int i = 1; i <= 2; ++i) {
		relaunch();
		CHECK(!exists(uiSet, Mode));
		p4Bit7(("after relaunch " + std::to_string(i)).c_str());
		p4UIRest();
		p4Same(p4, before, "after relaunch");
	}
}

// B: remove Mode, add it back at a custom spot and hidden -> persists x2, bit 7 cleared
CASE("p4 B add Mode back with custom pos/hide, persists after relaunch x2") {
	p4Start();
	removeBtn(uiSet, Mode);
	relaunch();
	addBack(uiSet, Mode);
	Pos want{-15, -12};
	moveBtn(uiSet, Mode, want);
	hideBtn(uiSet, Mode, true);
	for(int i = 1; i <= 2; ++i) {
		relaunch();
		CHECK_MSG(exists(uiSet, Mode), "re-added Mode missing after relaunch " + std::to_string(i));
		CHECK(pos(uiSet, Mode) == want);
		CHECK(hidden(uiSet, Mode));
		auto cfg = save();
		for(int k : {OpenMenu, FastForward, Rewind})
			CHECK_MSG(!(savedDuoFlags(cfg, uiSet, k) & removedFlag), std::string("bit 7 still set on ") + keyName(k) + " after add back");
	}
}

// C: move/hide Mode and several P4 buttons (incl. Start = last key), persist x2
CASE("p4 C move/hide Mode and P4 pads, persist after relaunch x2") {
	p4Start();
	P4Snap all[numSets]; p4SnapAll(all);
	Pos mWant{8, 20}, aWant = all[p4].p[A], sWant = all[p4].p[Start];
	aWant.x += 12; aWant.y -= 9; sWant.x -= 7; sWant.y += 5;
	moveBtn(uiSet, Mode, mWant);
	hideBtn(uiSet, Mode, true);
	moveBtn(p4, A, aWant);
	moveBtn(p4, Start, sWant);
	hideBtn(p4, Z, true);
	for(int i = 1; i <= 2; ++i) {
		relaunch();
		CHECK(exists(uiSet, Mode) && pos(uiSet, Mode) == mWant && hidden(uiSet, Mode));
		CHECK_MSG(pos(p4, A) == aWant, "P4 A move lost");
		CHECK_MSG(pos(p4, Start) == sWant, "P4 Start move lost");
		CHECK_MSG(hidden(p4, Z), "P4 Z hide lost");
		for(int k : gameKeys()) if(k != A && k != Start && k != Z) {
			CHECK(exists(p4, k) == all[p4].ex[k]);
			if(all[p4].ex[k]) CHECK(pos(p4, k) == all[p4].p[k]);
		}
		p4AllSame(all, "after P4 edits", p4);
		p4RolesSaved("after relaunch");
	}
}

// D: Reset UI Groups restores Mode and clears the removed mark. No helper yet.
CASE("p4 D Reset UI Groups restores Mode at defaults, clears removed mark") {
	p4Start();
	P4Snap all[numSets]; p4SnapAll(all);
	Pos uiPos[4]; bool uiHid[4]; int i = 0;
	for(int k : defaultUIKeys()) { uiPos[i] = pos(uiSet, k); uiHid[i] = hidden(uiSet, k); ++i; }
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	moveBtn(uiSet, Rewind, {-33, 44});
	hideBtn(uiSet, FastForward, true);
	moveBtn(p4, B, {all[p4].p[B].x + 5, all[p4].p[B].y + 5});
	closeLayoutEditor();
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "precondition: Mode should be gone before reset");
	Pos bMoved = pos(p4, B);
	resetUIGroups();
	closeLayoutEditor();
	for(int r = 0; r < 3; ++r) {
		const char *when = r == 0 ? "right after reset" : "after reset + relaunch";
		auto cfg = save();
		i = 0;
		for(int k : defaultUIKeys()) {
			CHECK_MSG(exists(uiSet, k), std::string(keyName(k)) + " missing " + when);
			CHECK_MSG(pos(uiSet, k) == uiPos[i], std::string(keyName(k)) + " not at default pos " + when);
			CHECK_MSG(hidden(uiSet, k) == uiHid[i], std::string(keyName(k)) + " hidden state not default " + when);
			++i;
		}
		for(int k : {OpenMenu, FastForward, Rewind})
			CHECK_MSG(!(savedDuoFlags(cfg, uiSet, k) & removedFlag), std::string("removed mark still on ") + keyName(k) + " " + when);
		CHECK_MSG(pos(p4, B) == bMoved, std::string("Reset UI Groups touched P4 pads ") + when);
		p4AllSame(all, when, p4);
		if(r < 2) relaunch();
	}
}

// E: NEO-Duo 2-player config has roles 0..1 only. P4 (role 3) must get full defaults,
//    save with role 3, and P1/P2 edits from the old file must not leak into P4.
CASE("p4 E old NEO-Duo 2P config: P4 gets defaults with role 3, Mode shown") {
	loadConfig(neoDuo2PlayerConfig());
	setPlayers(4);
	CHECK(players() == 4);
	CHECK_MSG(exists(uiSet, Mode), "Mode missing after loading NEO-Duo 2P config");
	for(int k : gameKeys())
		CHECK_MSG(exists(p4, k), std::string("P4 ") + keyName(k) + " missing after old config");
	p4RolesSaved("after old config load");
	relaunch(2);
	for(int k : gameKeys()) CHECK(exists(p4, k));
	p4RolesSaved("after old config + relaunch x2");
}

// F: edit in layout editor, close, kill -> Mode removal and P4 move survive
CASE("p4 F close layout editor saves; kill keeps Mode removal and P4 R move") {
	p4Start();
	auto before = p4Snap(p4);
	Pos want{before.p[R].x - 10, before.p[R].y + 6};
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	moveBtn(p4, R, want);
	closeLayoutEditor();
	killAfterEditor();
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back after kill (editor close did not save)");
	CHECK_MSG(pos(p4, R) == want, "P4 R move lost after kill");
	relaunch();
	CHECK(!exists(uiSet, Mode));
	CHECK(pos(p4, R) == want);
}

// G: remove Mode, switch layout, relaunch, switch back, relaunch -> stays gone
CASE("p4 G remove Mode, switch layout, relaunch x2, stays gone, P4 intact") {
	p4Start();
	auto before = p4Snap(p4);
	removeBtn(uiSet, Mode);
	switchLayout();
	relaunch();
	CHECK(!exists(uiSet, Mode));
	switchLayout();
	relaunch();
	CHECK(!exists(uiSet, Mode));
	p4UIRest();
	for(int k : gameKeys()) CHECK(exists(p4, k) == before.ex[k]);
}

// G2: as G, bit 7 checked in saved bytes after every switch and relaunch
CASE("p4 G2 default UI: remove Mode, switch layout, relaunch x2, bit7 stays, P4 intact") {
	p4Start();
	auto before = p4Snap(p4);
	removeBtn(uiSet, Mode);
	for(int i = 1; i <= 2; ++i) {
		switchLayout();
		p4Bit7(("after switch " + std::to_string(i)).c_str());
		relaunch();
		CHECK(!exists(uiSet, Mode));
		p4Bit7(("after relaunch " + std::to_string(i)).c_str());
	}
	for(int n : {2, 1, 4}) {
		setPlayers(n);
		CHECK_MSG(!exists(uiSet, Mode), "Mode came back after setPlayers(" + std::to_string(n) + ")");
		p4Bit7(("after setPlayers(" + std::to_string(n) + ")").c_str());
		relaunch();
		CHECK(!exists(uiSet, Mode));
		p4Bit7(("after setPlayers(" + std::to_string(n) + ") + relaunch").c_str());
	}
	CHECK(players() == 4);
	p4UIRest();
	for(int k : gameKeys()) CHECK(exists(p4, k) == before.ex[k]);
}

// H: remove all UI, relaunch x2 -> nothing regrows, P4 pads intact
CASE("p4 H remove all UI, relaunch x2, no crash/no regrow, P4 pads intact") {
	p4Start();
	P4Snap all[numSets]; p4SnapAll(all);
	removeAllUI();
	relaunch(2);
	CHECK_MSG(count(uiSet) == 0, "UI buttons regrew after removeAllUI + relaunch x2");
	p4AllSame(all, "after removeAllUI");
}

// H-pad: remove every P4 game button (empty set), relaunch x2 -> must stay empty.
// Same class as cross 3c: "user emptied set 4" must not be read as "old file without set 4".
CASE("p4 H-pad remove all P4 pads, relaunch x2, set stays empty, others intact") {
	p4Start();
	P4Snap all[numSets]; p4SnapAll(all);
	for(int k : gameKeys()) removeBtn(p4, k);
	CHECK(count(p4) == 0);
	relaunch(2);
	CHECK_MSG(count(p4) == 0, "P4 buttons regrew (empty set treated as old config)");
	p4AllSame(all, "after emptying P4", p4);
	for(int k : defaultUIKeys()) CHECK(exists(uiSet, k));
}

// I (fix required this round, normal CASE): remove all UI, add back a non-Mode button, relaunch x2 -> Mode should stay gone
CASE("p4 I remove all UI, add back non-Mode, relaunch x2, Mode stays gone") {
	p4Start();
	removeAllUI();
	relaunch();
	addBack(uiSet, OpenMenu);
	relaunch(2);
	CHECK(exists(uiSet, OpenMenu));
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back after re-adding a non-Mode UI button");
}

// A-port: after Mode removal + relaunch x2, P4 pads still drive Saturn port 4 only
CASE("p4 A-port remove Mode, relaunch x2, P4 pads reach port 4 only") {
	p4Start();
	removeBtn(uiSet, Mode);
	relaunch(2);
	for(int k : {A, Start, Dpad}) {
		auto in = press(p4, k);
		CHECK_MSG(in[3] != 0, std::string("P4 ") + keyName(k) + " did not reach port 4");
		for(int i = 0; i < 3; ++i)
			CHECK_MSG(in[i] == 0, std::string("P4 ") + keyName(k) + " leaked to port " + std::to_string(i + 1));
	}
}
