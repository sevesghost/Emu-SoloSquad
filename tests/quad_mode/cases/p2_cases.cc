// P2 (player index 1, duoRole 1, Saturn port 2) cases. Owner: west-2.
// Include-only header, official helpers, names "p2 " + NEO-Duo letter (README).
// Start from freshInstall() — real default UI (OpenMenu, FastForward+Rewind, Mode),
// not the Mode-only layout. Do not fake a pass; real 4P code is not in tree yet
// (real backend marks every case PENDING). Model self-check is labelled MODEL.
#include "quad_harness.hh"
using namespace quad;

constexpr int p2 = 1; // P2

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
static void othersIntact(const Snap all[numSets], const char *when, int exceptSet = p2) {
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

// Saved-bytes snapshot used by A-all: pos/hidden/duoFlags/duoRole per button (from BYTES, not only memory).
struct BtnSnap { bool ex = false, hid = false; Pos p; uint8_t flags = 0, role = 0; bool operator==(const BtnSnap &) const = default; };
static BtnSnap snapBtn(int s, int k, const std::vector<uint8_t> &cfg) {
	bool ex = exists(s, k);
	return {ex, ex && hidden(s, k), ex ? pos(s, k) : Pos{}, ex ? savedDuoFlags(cfg, s, k) : uint8_t(0),
		ex ? savedDuoRole(cfg, s, k) : uint8_t(0)};
}
static void expectBtnSame(int s, int k, const BtnSnap &want, const std::vector<uint8_t> &cfg, const char *when) {
	auto now = snapBtn(s, k, cfg);
	const char *who = (s == uiSet) ? "UI" : "P";
	std::string id = std::string(who) + (s == uiSet ? std::string() : std::to_string(s + 1)) + " " + keyName(k);
	CHECK_MSG(now == want, id + " pos/hidden/duoFlags/duoRole changed " + when);
}


// A: remove shared Mode (three-line), relaunch x2 -> stays gone; P2 pads intact
CASE("p2 A remove Mode, relaunch x2, stays gone, P2 pads intact") {
	startDefault();
	Snap all[numSets]; FOR_SETS(s) all[s] = snap(s);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	closeLayoutEditor();
	CHECK(!exists(uiSet, Mode));
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after relaunch 1: Mode came back");
	remainingUIPresent();
	sameAs(p2, all[p2], "after relaunch 1");
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after relaunch 2: Mode came back");
	remainingUIPresent();
	sameAs(p2, all[p2], "after relaunch 2");
	othersIntact(all, "after Mode delete");
}

// A-pad: same persistence for a P2 game button (lead: delete a button, including Mode)
CASE("p2 A-pad remove P2 Start, relaunch x2, stays gone, others intact") {
	startDefault();
	Snap all[numSets]; FOR_SETS(s) all[s] = snap(s);
	int before = count(p2);
	openLayoutEditor();
	removeBtn(p2, Start);
	closeLayoutEditor();
	CHECK(!exists(p2, Start));
	relaunch(2);
	CHECK_MSG(!exists(p2, Start), "P2 Start came back");
	CHECK(count(p2) == before - 1);
	for(int k : gameKeys()) if(k != Start)
		CHECK_MSG(exists(p2, k), std::string("P2 lost ") + keyName(k));
	othersIntact(all, "after deleting P2 Start");
	for(int k : defaultUIKeys()) CHECK(exists(uiSet, k));
}

// A-all: remove every P2 pad button, relaunch x2 -> P2 stays empty.
// P1/P3/P4 and UI must be unchanged (per-button pos/hidden/duoFlags/duoRole in memory + saved bytes).
// Normal CASE (lead: real bug, not XFAIL). Model now writes a 'user removed whole set' marker;
// QUAD_MODEL_BUG=no_set_marker restores the old treat-empty-as-missing-2P-set behavior.
CASE("p2 A-all remove every P2 pad button, relaunch x2, P2 stays empty, others+UI unchanged") {
	startDefault();
	auto cfg0 = save();
	BtnSnap pads[numSets][16]{};
	BtnSnap uiBefore[8]{};
	int uiKeys[8]{};
	int nUI = 0;
	FOR_SETS(s) for(int k : gameKeys()) pads[s][k] = snapBtn(s, k, cfg0);
	for(int k : defaultUIKeys()) { uiKeys[nUI] = k; uiBefore[nUI] = snapBtn(uiSet, k, cfg0); ++nUI; }
	openLayoutEditor();
	for(int k : gameKeys()) removeBtn(p2, k);
	closeLayoutEditor();
	CHECK(count(p2) == 0);
	auto expectOthers = [&](const char *when) {
		auto cfg = save();
		CHECK_MSG(count(p2) == 0, std::string("P2 not empty ") + when);
		for(int k : gameKeys())
			CHECK_MSG(!exists(p2, k), std::string("P2 ") + keyName(k) + " came back " + when);
		FOR_SETS(s) if(s != p2)
			for(int k : gameKeys())
				expectBtnSame(s, k, pads[s][k], cfg, when);
		for(int i = 0; i < nUI; ++i)
			expectBtnSame(uiSet, uiKeys[i], uiBefore[i], cfg, when);
	};
	expectOthers("right after remove");
	relaunch();
	expectOthers("after relaunch 1");
	relaunch();
	expectOthers("after relaunch 2");
}

// A2: A from the real default UI set; remaining UI carries bit 7 in saved BYTES
CASE("p2 A2 default UI: remove Mode, relaunch x2, bit7 on remaining UI, P2 intact") {
	startDefault();
	auto before = snap(p2);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	closeLayoutEditor();
	CHECK(!exists(uiSet, Mode));
	bit7OnRemainingUI("right after save");
	relaunch();
	modeGoneCarried("after relaunch 1");
	sameAs(p2, before, "after relaunch 1");
	relaunch();
	modeGoneCarried("after relaunch 2");
	sameAs(p2, before, "after relaunch 2");
}

// B: remove Mode, add back with custom pos + hidden, relaunch x2 -> kept exactly
CASE("p2 B add Mode back with custom pos/hide, persists after relaunch x2") {
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
	CHECK(exists(p2, A));
}

// C: move + hide Mode and a P2 game button; values persist after relaunch x2
CASE("p2 C move/hide Mode and P2 B, persist after relaunch x2") {
	startDefault();
	Snap all[numSets]; FOR_SETS(s) all[s] = snap(s);
	Pos modeWant{-15, -12};
	Pos bWant{8, 20};
	openLayoutEditor();
	moveBtn(uiSet, Mode, modeWant);
	hideBtn(uiSet, Mode, true);
	moveBtn(p2, B, bWant);
	hideBtn(p2, B, true);
	closeLayoutEditor();
	relaunch();
	CHECK(exists(uiSet, Mode) && exists(p2, B));
	CHECK_MSG(pos(uiSet, Mode) == modeWant, "Mode move lost after relaunch 1");
	CHECK_MSG(hidden(uiSet, Mode), "Mode hide lost after relaunch 1");
	CHECK_MSG(pos(p2, B) == bWant, "P2 B move lost after relaunch 1");
	CHECK_MSG(hidden(p2, B), "P2 B hide lost after relaunch 1");
	relaunch();
	CHECK(exists(uiSet, Mode) && exists(p2, B));
	CHECK_MSG(pos(uiSet, Mode) == modeWant, "Mode move lost after relaunch 2");
	CHECK_MSG(hidden(uiSet, Mode), "Mode hide lost after relaunch 2");
	CHECK_MSG(pos(p2, B) == bWant, "P2 B move lost after relaunch 2");
	CHECK_MSG(hidden(p2, B), "P2 B hide lost after relaunch 2");
	othersIntact(all, "after move/hide on P2/Mode");
}

// D: Reset UI Groups restores default UI including Mode and clears the removed mark.
CASE("p2 D Reset UI Groups restores Mode at defaults, clears removed mark") {
	startDefault();
	auto cfg0 = save();
	auto p2before = snap(p2);
	BtnSnap uiDef[8]{};
	int uiKeys[8]{};
	int nUI = 0;
	for(int k : defaultUIKeys()) { uiKeys[nUI] = k; uiDef[nUI] = snapBtn(uiSet, k, cfg0); ++nUI; }
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	moveBtn(uiSet, OpenMenu, {111, 222});
	hideBtn(uiSet, OpenMenu, true);
	closeLayoutEditor();
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "precondition: Mode should still be gone before reset");
	resetUIGroups();
	closeLayoutEditor();
	auto cfg = save();
	for(int i = 0; i < nUI; ++i)
		expectBtnSame(uiSet, uiKeys[i], uiDef[i], cfg, "after Reset UI Groups");
	CHECK(exists(uiSet, Mode));
	CHECK_MSG(!(savedDuoFlags(cfg, uiSet, OpenMenu) & removedFlag), "removed mark still set after Reset UI Groups");
	sameAs(p2, p2before, "after Reset UI Groups");
	relaunch();
	cfg = save();
	for(int i = 0; i < nUI; ++i)
		expectBtnSame(uiSet, uiKeys[i], uiDef[i], cfg, "after reset + relaunch");
	CHECK(exists(uiSet, Mode));
	CHECK_MSG(!(savedDuoFlags(cfg, uiSet, OpenMenu) & removedFlag), "removed mark returned after relaunch");
	sameAs(p2, p2before, "after reset + relaunch");
}

// E: old NEO-Duo 2-player config loads; P2 (role 1) and Mode present; roles 2..3 default
CASE("p2 E old NEO-Duo 2P config loads P2 and Mode; P3/P4 default") {
	loadConfig(neoDuo2PlayerConfig());
	CHECK_MSG(exists(uiSet, Mode), "Mode missing after loading 2P config");
	for(int k : gameKeys())
		CHECK_MSG(exists(p2, k), std::string("P2 missing ") + keyName(k) + " from 2P config");
	CHECK(count(p2) == (int)gameKeys().size());
	CHECK_MSG(savedDuoRole(save(), p2, A) == p2, "P2 duoRole is not 1");
	// roles 2..3 were absent in the 2P blob and must get defaults
	for(int s : {2, 3})
		for(int k : gameKeys())
			CHECK_MSG(exists(s, k), std::string("P") + std::to_string(s + 1) + " did not get defaults from old 2P config");
	relaunch();
	CHECK(exists(uiSet, Mode));
	for(int k : gameKeys()) CHECK(exists(p2, k));
}

// F: editor close must save; kill right after close keeps Mode removal + a P2 move
CASE("p2 F close layout editor saves; kill keeps Mode removal and P2 A move") {
	startDefault();
	auto before = snap(p2);
	Pos want{before.p[A].x + 10, before.p[A].y - 8};
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	moveBtn(p2, A, want);
	closeLayoutEditor();
	killAfterEditor();
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back after kill (editor close did not save)");
	CHECK_MSG(pos(p2, A) == want, "P2 A move lost after kill");
	relaunch();
	CHECK(!exists(uiSet, Mode));
	CHECK(pos(p2, A) == want);
	for(int k : gameKeys()) if(k != A) {
		CHECK(exists(p2, k) == before.ex[k]);
		if(before.ex[k]) CHECK(pos(p2, k) == before.p[k]);
	}
}

// G: remove Mode, switch Joint/Separate, relaunch, switch back, relaunch -> stays gone
CASE("p2 G remove Mode, switch layout, relaunch x2, stays gone, P2 intact") {
	startDefault();
	auto before = snap(p2);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	closeLayoutEditor();
	switchLayout();
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back right after layout switch");
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after switch + relaunch 1: Mode came back");
	sameAs(p2, before, "after switch + relaunch 1");
	switchLayout();
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "after switch back + relaunch 2: Mode came back");
	sameAs(p2, before, "after switch back + relaunch 2");
	remainingUIPresent();
}

// G2: G from real default set; bit 7 still in saved bytes after every switch/relaunch
CASE("p2 G2 default UI: remove Mode, switch layout, relaunch x2, bit7 stays, P2 intact") {
	startDefault();
	auto before = snap(p2);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	switchLayout();
	closeLayoutEditor();
	bit7OnRemainingUI("after switch, before relaunch");
	CHECK(!exists(uiSet, Mode));
	relaunch();
	modeGoneCarried("G2 after relaunch 1");
	sameAs(p2, before, "G2 after relaunch 1");
	switchLayout();
	auto cfg = save();
	CHECK(savedDuoFlags(cfg, uiSet, OpenMenu) & removedFlag);
	relaunch();
	modeGoneCarried("G2 after relaunch 2");
	sameAs(p2, before, "G2 after relaunch 2");
	for(int n : {2, 1, 4}) {
		setPlayers(n);
		CHECK_MSG(!exists(uiSet, Mode), "Mode came back after setPlayers(" + std::to_string(n) + ")");
		bit7OnRemainingUI(("after setPlayers(" + std::to_string(n) + ")").c_str());
		relaunch();
		modeGoneCarried(("after setPlayers(" + std::to_string(n) + ") + relaunch").c_str());
	}
	CHECK(players() == 4);
	sameAs(p2, before, "G2 after player-count switches");
}

// H: remove every UI button, relaunch x2 -> no crash, nothing regrows; P2 pads intact
CASE("p2 H remove all UI, relaunch x2, no crash/no regrow, P2 pads intact") {
	startDefault();
	auto before = snap(p2);
	openLayoutEditor();
	removeAllUI();
	closeLayoutEditor();
	CHECK(count(uiSet) == 0);
	relaunch();
	CHECK_MSG(count(uiSet) == 0, "UI elements reappeared after relaunch 1");
	CHECK(!exists(uiSet, Mode));
	sameAs(p2, before, "after deleting all UI + relaunch 1");
	relaunch();
	CHECK_MSG(count(uiSet) == 0, "UI elements reappeared after relaunch 2");
	CHECK(!exists(uiSet, Mode));
	sameAs(p2, before, "after deleting all UI + relaunch 2");
}

// I: remove all UI, relaunch, add back a non-Mode button, relaunch -> Mode must stay gone.
// Lead: North will fix this known issue this round; user requires no button bugs. Normal CASE.
CASE("p2 I remove all UI, add back non-Mode, relaunch x2, Mode stays gone") {
	startDefault();
	auto before = snap(p2);
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
	sameAs(p2, before, "after I");
}
