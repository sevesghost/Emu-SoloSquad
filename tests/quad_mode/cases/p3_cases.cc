// P3 (player index 2, duoRole 2, Saturn port 3) cases. Owner: west-3.
// Include-only header, official helpers, names "p3 " + NEO-Duo letter (README).
// Start from freshInstall() — real default UI (OpenMenu, FastForward+Rewind, Mode),
// not the Mode-only layout. Do not fake a pass; real 4P code is not in tree yet
// (real backend marks every case PENDING). Model self-check is labelled MODEL.
#include "quad_harness.hh"
using namespace quad;

constexpr int p3 = 2; // P3

static void startDefault() {
	freshInstall();
	setPlayers(4);
	CHECK(players() == 4);
	for(int k : defaultUIKeys())
		CHECK_MSG(exists(uiSet, k), std::string("missing default UI ") + keyName(k));
	FOR_SETS(s)
		for(int k : gameKeys())
			CHECK_MSG(exists(s, k), std::string("missing ") + keyName(k) + " on set " + std::to_string(s));
}

struct Snap { bool ex[16]{}, hid[16]{}; Pos p[16]; };
static Snap snap(int s) {
	Snap n{};
	for(int k : gameKeys()) {
		n.ex[k] = exists(s, k);
		n.hid[k] = hidden(s, k);
		if(n.ex[k]) n.p[k] = pos(s, k);
	}
	return n;
}
static void sameAs(int s, const Snap &a, const char *when) {
	for(int k : gameKeys()) {
		CHECK_MSG(exists(s, k) == a.ex[k], std::string("P") + std::to_string(s + 1) + " " + keyName(k) + " presence changed " + when);
		CHECK_MSG(hidden(s, k) == a.hid[k], std::string("P") + std::to_string(s + 1) + " " + keyName(k) + " hidden changed " + when);
		if(a.ex[k]) CHECK_MSG(pos(s, k) == a.p[k], std::string("P") + std::to_string(s + 1) + " " + keyName(k) + " moved " + when);
	}
}
static void othersIntact(const Snap all[numSets], const char *when, int exceptSet = p3) {
	FOR_SETS(s) if(s != exceptSet) sameAs(s, all[s], when);
}
static void remainingUIPresent() {
	CHECK(exists(uiSet, OpenMenu));
	CHECK(exists(uiSet, FastForward));
	CHECK(exists(uiSet, Rewind));
}
static void bit7OnRemainingUI(const char *when) {
	auto cfg = save();
	for(int k : {OpenMenu, FastForward, Rewind})
		CHECK_MSG(savedDuoFlags(cfg, uiSet, k) & removedFlag,
			std::string("bit 7 (MODE_REMOVED) missing on saved ") + keyName(k) + " " + when);
}
static void modeGoneCarried(const char *when) {
	CHECK_MSG(!exists(uiSet, Mode), std::string("Mode button came back ") + when);
	bit7OnRemainingUI(when);
	remainingUIPresent();
}

// A: remove shared Mode (three-line), relaunch x2 -> stays gone; P3 pads intact
CASE("p3 A remove Mode, relaunch x2, stays gone, P3 pads intact") {
	startDefault();
	Snap all[numSets]; FOR_SETS(s) all[s] = snap(s);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	closeLayoutEditor();
	CHECK(!exists(uiSet, Mode));
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after relaunch 1: Mode came back");
	remainingUIPresent();
	sameAs(p3, all[p3], "after relaunch 1");
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after relaunch 2: Mode came back");
	remainingUIPresent();
	sameAs(p3, all[p3], "after relaunch 2");
	othersIntact(all, "after Mode delete");
}

// A-pad: same persistence for a P3 game button (Z) (lead: delete a button, including Mode)
CASE("p3 A-pad remove P3 Z, relaunch x2, stays gone, others intact") {
	startDefault();
	Snap all[numSets]; FOR_SETS(s) all[s] = snap(s);
	int before = count(p3);
	openLayoutEditor();
	removeBtn(p3, Z);
	closeLayoutEditor();
	CHECK(!exists(p3, Z));
	relaunch(2);
	CHECK_MSG(!exists(p3, Z), "P3 Z came back");
	CHECK(count(p3) == before - 1);
	for(int k : gameKeys()) if(k != Z)
		CHECK_MSG(exists(p3, k), std::string("P3 lost ") + keyName(k));
	othersIntact(all, "after deleting P3 Z");
	for(int k : defaultUIKeys()) CHECK(exists(uiSet, k));
}

// A-set: remove every P3 game button (whole set), relaunch x2 -> stays empty, not refilled as "old config" (same rule as cross 3c)
CASE("p3 A-set remove all P3 buttons, relaunch x2, stays empty, others intact") {
	startDefault();
	Snap all[numSets]; FOR_SETS(s) all[s] = snap(s);
	openLayoutEditor();
	for(int k : gameKeys()) removeBtn(p3, k);
	closeLayoutEditor();
	CHECK(count(p3) == 0);
	relaunch();
	CHECK_MSG(count(p3) == 0, "P3 buttons came back after relaunch 1");
	relaunch();
	CHECK_MSG(count(p3) == 0, "P3 buttons came back after relaunch 2");
	othersIntact(all, "after deleting all P3 buttons");
	for(int k : defaultUIKeys()) CHECK(exists(uiSet, k));
}

// A2: A from the real default UI set; remaining UI carries bit 7 in saved BYTES
CASE("p3 A2 default UI: remove Mode, relaunch x2, bit7 on remaining UI, P3 intact") {
	startDefault();
	auto before = snap(p3);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	closeLayoutEditor();
	CHECK(!exists(uiSet, Mode));
	bit7OnRemainingUI("right after save");
	relaunch();
	modeGoneCarried("after relaunch 1");
	sameAs(p3, before, "after relaunch 1");
	relaunch();
	modeGoneCarried("after relaunch 2");
	sameAs(p3, before, "after relaunch 2");
}

// B: remove Mode, add back with custom pos + hidden, relaunch x2 -> kept exactly
CASE("p3 B add Mode back with custom pos/hide, persists after relaunch x2") {
	startDefault();
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	closeLayoutEditor();
	relaunch();
	if(exists(uiSet, Mode)) removeBtn(uiSet, Mode);
	addBack(uiSet, Mode);
	Pos want{-120, -80};
	moveBtn(uiSet, Mode, want);
	hideBtn(uiSet, Mode, true);
	closeLayoutEditor();
	relaunch(2);
	CHECK_MSG(exists(uiSet, Mode), "re-added Mode lost after relaunch");
	CHECK_MSG(pos(uiSet, Mode) == want, "Mode position not kept");
	CHECK_MSG(hidden(uiSet, Mode), "Mode hidden state not kept");
	CHECK(exists(p3, A));
}

// C: move + hide Mode and a P3 game button; values persist after relaunch x2
CASE("p3 C move/hide Mode and P3 X, persist after relaunch x2") {
	startDefault();
	Snap all[numSets]; FOR_SETS(s) all[s] = snap(s);
	Pos modeWant{-15, -12};
	Pos bWant{8, 20};
	openLayoutEditor();
	moveBtn(uiSet, Mode, modeWant);
	hideBtn(uiSet, Mode, true);
	moveBtn(p3, X, bWant);
	hideBtn(p3, X, true);
	closeLayoutEditor();
	relaunch();
	CHECK(exists(uiSet, Mode) && exists(p3, X));
	CHECK_MSG(pos(uiSet, Mode) == modeWant, "Mode move lost after relaunch 1");
	CHECK_MSG(hidden(uiSet, Mode), "Mode hide lost after relaunch 1");
	CHECK_MSG(pos(p3, X) == bWant, "P3 X move lost after relaunch 1");
	CHECK_MSG(hidden(p3, X), "P3 X hide lost after relaunch 1");
	relaunch();
	CHECK(exists(uiSet, Mode) && exists(p3, X));
	CHECK_MSG(pos(uiSet, Mode) == modeWant, "Mode move lost after relaunch 2");
	CHECK_MSG(hidden(uiSet, Mode), "Mode hide lost after relaunch 2");
	CHECK_MSG(pos(p3, X) == bWant, "P3 X move lost after relaunch 2");
	CHECK_MSG(hidden(p3, X), "P3 X hide lost after relaunch 2");
	othersIntact(all, "after move/hide on P3/Mode");
}

// D: Reset UI Groups after Mode removal (+ OpenMenu moved/hidden + a P3 move) -> default UI incl. Mode, bit 7 cleared, P3 untouched
CASE("p3 D Reset UI Groups restores Mode/UI defaults, clears bit7, keeps P3 edits") {
	startDefault();
	auto cfg0 = save();
	Pos menuDef = pos(uiSet, OpenMenu), modeDef = pos(uiSet, Mode);
	Pos yWant{pos(p3, Y).x + 12, pos(p3, Y).y - 6};
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	moveBtn(uiSet, OpenMenu, {70, 140});
	hideBtn(uiSet, OpenMenu, true);
	moveBtn(p3, Y, yWant);
	closeLayoutEditor();
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "precondition: Mode back before reset");
	auto p3before = snap(p3);
	resetUIGroups();
	closeLayoutEditor();
	for(int r = 0; r <= 2; ++r) {
		std::string when = r == 0 ? "right after reset" : "after reset + relaunch " + std::to_string(r);
		auto cfg = save();
		for(int k : defaultUIKeys()) CHECK_MSG(exists(uiSet, k), std::string("UI ") + keyName(k) + " missing " + when);
		CHECK_MSG(pos(uiSet, Mode) == modeDef, "Mode not at default " + when);
		CHECK_MSG(!hidden(uiSet, Mode), "Mode hidden " + when);
		CHECK_MSG(pos(uiSet, OpenMenu) == menuDef, "OpenMenu not back at default " + when);
		CHECK_MSG(!hidden(uiSet, OpenMenu), "OpenMenu still hidden " + when);
		for(int k : {OpenMenu, FastForward, Rewind})
			CHECK_MSG(!(savedDuoFlags(cfg, uiSet, k) & removedFlag), std::string("bit 7 still on ") + keyName(k) + " " + when);
		sameAs(p3, p3before, when.c_str());
		CHECK_MSG(pos(p3, Y) == yWant, "P3 Y move lost " + when);
		if(r < 2) relaunch();
	}
	(void)cfg0;
}

// E: old NEO-Duo 2-player config (roles 0..1 only) loads; P3 (role 2) absent -> full defaults, role 2, Mode present
CASE("p3 E old NEO-Duo 2P config loads; P3 gets full defaults with role 2; Mode present") {
	loadConfig(neoDuo2PlayerConfig());
	CHECK_MSG(exists(uiSet, Mode), "Mode missing after loading 2P config");
	for(int k : gameKeys())
		CHECK_MSG(exists(p3, k), std::string("P3 did not get default ") + keyName(k) + " from old 2P config");
	CHECK(count(p3) == (int)gameKeys().size());
	CHECK_MSG(!hidden(p3, A), "P3 default A is hidden");
	for(int s : {0, 1})
		for(int k : gameKeys())
			CHECK_MSG(exists(s, k), std::string("P") + std::to_string(s + 1) + " lost " + keyName(k) + " from 2P config");
	auto cfg = save();
	CHECK_MSG(savedDuoRole(cfg, p3, A) == p3, "P3 duoRole is not 2 after save");
	relaunch();
	CHECK(exists(uiSet, Mode));
	for(int k : gameKeys()) CHECK(exists(p3, k));
	CHECK_MSG(savedDuoRole(save(), p3, A) == p3, "P3 duoRole not 2 after relaunch");
}

// P3 identity: pad tint differs from P1/P2/P4 and survives relaunch (needs duoRoleColor in real code)
CASE("p3 color P3 pad tint distinct from others and kept after relaunch x2") {
	startDefault();
	Rgb c = color(p3);
	FOR_SETS(s) if(s != p3) CHECK_MSG(!(color(s) == c), std::string("P3 tint equals P") + std::to_string(s + 1));
	relaunch(2);
	CHECK_MSG(color(p3) == c, "P3 tint changed after relaunch");
}

// F: editor close must save; kill right after close keeps Mode removal + a P3 move
CASE("p3 F close layout editor saves; kill keeps Mode removal and P3 A move") {
	startDefault();
	auto before = snap(p3);
	Pos want{before.p[A].x + 10, before.p[A].y - 8};
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	moveBtn(p3, A, want);
	closeLayoutEditor();
	killAfterEditor();
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back after kill (editor close did not save)");
	CHECK_MSG(pos(p3, A) == want, "P3 A move lost after kill");
	relaunch();
	CHECK(!exists(uiSet, Mode));
	CHECK(pos(p3, A) == want);
	for(int k : gameKeys()) if(k != A) {
		CHECK(exists(p3, k) == before.ex[k]);
		if(before.ex[k]) CHECK(pos(p3, k) == before.p[k]);
	}
}

// G: remove Mode, switch Joint/Separate, relaunch, switch back, relaunch -> stays gone
CASE("p3 G remove Mode, switch layout, relaunch x2, stays gone, P3 intact") {
	startDefault();
	auto before = snap(p3);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	closeLayoutEditor();
	switchLayout();
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back right after layout switch");
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after switch + relaunch 1: Mode came back");
	sameAs(p3, before, "after switch + relaunch 1");
	switchLayout();
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after switch back + relaunch 2: Mode came back");
	sameAs(p3, before, "after switch back + relaunch 2");
	remainingUIPresent();
}

// G2: G from real default set; bit 7 still in saved bytes after every switch/relaunch
CASE("p3 G2 default UI: remove Mode, switch layout + players 2/1/4, relaunch x2, bit7 stays, P3 intact") {
	startDefault();
	auto before = snap(p3);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	switchLayout();
	closeLayoutEditor();
	bit7OnRemainingUI("after switch, before relaunch");
	CHECK(!exists(uiSet, Mode));
	relaunch();
	modeGoneCarried("G2 after relaunch 1");
	sameAs(p3, before, "G2 after relaunch 1");
	for(int n : {2, 1, 4}) { // player-count switch (4p/2p/1p) must not drop bit 7 or resurrect Mode
		setPlayers(n);
		CHECK(players() == n);
		CHECK_MSG(!exists(uiSet, Mode), "Mode came back after setPlayers(" + std::to_string(n) + ")");
		bit7OnRemainingUI(("after setPlayers(" + std::to_string(n) + ")").c_str());
	}
	sameAs(p3, before, "G2 after player-count switches");
	switchLayout();
	auto cfg = save();
	CHECK(savedDuoFlags(cfg, uiSet, OpenMenu) & removedFlag);
	relaunch();
	modeGoneCarried("G2 after relaunch 2");
	sameAs(p3, before, "G2 after relaunch 2");
}

// H: remove every UI button, relaunch x2 -> no crash, nothing regrows; P3 pads intact
CASE("p3 H remove all UI, relaunch x2, no crash/no regrow, P3 pads intact") {
	startDefault();
	auto before = snap(p3);
	openLayoutEditor();
	removeAllUI();
	closeLayoutEditor();
	CHECK(count(uiSet) == 0);
	relaunch();
	CHECK_MSG(count(uiSet) == 0, "UI elements reappeared after relaunch 1");
	CHECK(!exists(uiSet, Mode));
	sameAs(p3, before, "after deleting all UI + relaunch 1");
	relaunch();
	CHECK_MSG(count(uiSet) == 0, "UI elements reappeared after relaunch 2");
	CHECK(!exists(uiSet, Mode));
	sameAs(p3, before, "after deleting all UI + relaunch 2");
}

// I (must pass, north fixes it this round): remove all UI, relaunch, add back a non-Mode button, relaunch -> Mode must stay gone
// (NEO-Duo had this as a known issue: bit 7 has nowhere to live when the UI list is empty). Normal CASE now.
CASE("p3 I remove all UI, add back non-Mode, relaunch x2, Mode stays gone") {
	startDefault();
	auto before = snap(p3);
	openLayoutEditor();
	removeAllUI();
	closeLayoutEditor();
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after removing all + relaunch: Mode already back");
	addBack(uiSet, OpenMenu);
	closeLayoutEditor();
	relaunch();
	CHECK_MSG(exists(uiSet, OpenMenu), "re-added OpenMenu lost");
	CHECK_MSG(!exists(uiSet, Mode), "after re-adding OpenMenu + relaunch 1: Mode came back");
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after relaunch 2: Mode came back");
	sameAs(p3, before, "after I");
}
