// P1 (set 0, duoRole 0, Saturn port 1) cases. Owner: west-1.
// Start from freshInstall() (real default UI: OpenMenu, FastForward+Rewind, Mode by sync),
// never the Mode-only layout. Real backend is PENDING until the 4P code exists; model
// results only prove the checks are right. No faked passes.
#include "quad_harness.hh"
#include <map>
using namespace quad;

namespace {
constexpr int p1 = 0;

void startDefault() {
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
Snap snap(int s) {
	Snap n{};
	for(int k : gameKeys()) {
		n.ex[k] = exists(s, k);
		n.hid[k] = n.ex[k] && hidden(s, k);
		if(n.ex[k]) n.p[k] = pos(s, k);
	}
	return n;
}
void sameAs(int s, const Snap &a, const std::string &when) {
	auto tag = "P" + std::to_string(s + 1) + " ";
	for(int k : gameKeys()) {
		CHECK_MSG(exists(s, k) == a.ex[k], tag + keyName(k) + " presence changed " + when);
		if(!a.ex[k] || !exists(s, k)) continue;
		CHECK_MSG(hidden(s, k) == a.hid[k], tag + keyName(k) + " hidden changed " + when);
		CHECK_MSG(pos(s, k) == a.p[k], tag + keyName(k) + " moved " + when);
	}
}
struct All { Snap s[numSets]; };
All snapAll() { All a; FOR_SETS(s) a.s[s] = snap(s); return a; }
void othersIntact(const All &a, const std::string &when) { FOR_SETS(s) if(s != p1) sameAs(s, a.s[s], when); }
void bit7OnRemainingUI(const std::string &when) {
	auto cfg = save();
	for(int k : {OpenMenu, FastForward, Rewind})
		CHECK_MSG(savedDuoFlags(cfg, uiSet, k) & removedFlag,
			std::string("bit 7 missing on saved ") + keyName(k) + " " + when);
}
void modeGone(const std::string &when) {
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back " + when);
	for(int k : {OpenMenu, FastForward, Rewind})
		CHECK_MSG(exists(uiSet, k), std::string("lost UI ") + keyName(k) + " " + when);
}
}

CASE("p1 A remove Mode, relaunch x2, stays gone, P1 pad intact") {
	startDefault();
	auto all = snapAll();
	openLayoutEditor(); removeBtn(uiSet, Mode); closeLayoutEditor();
	for(int i = 1; i <= 2; i++) {
		relaunch();
		auto w = "after relaunch " + std::to_string(i);
		modeGone(w);
		sameAs(p1, all.s[p1], w);
	}
	othersIntact(all, "after Mode delete");
}

// every P1 game key, one at a time (lead: delete a button -> must not come back)
CASE("p1 A-pad remove each P1 game key, relaunch x2, stays gone, others intact") {
	for(int key : gameKeys()) {
		startDefault();
		auto all = snapAll();
		openLayoutEditor(); removeBtn(p1, key); closeLayoutEditor();
		relaunch(2);
		CHECK_MSG(!exists(p1, key), std::string("P1 ") + keyName(key) + " came back");
		for(int k : gameKeys()) if(k != key)
			CHECK_MSG(exists(p1, k), std::string("P1 lost ") + keyName(k) + " after deleting " + keyName(key));
		othersIntact(all, std::string("after deleting P1 ") + keyName(key));
		for(int k : defaultUIKeys()) CHECK_MSG(exists(uiSet, k), std::string("UI ") + keyName(k) + " changed");
	}
}

CASE("p1 A2 default UI: remove Mode, bit7 on remaining UI in saved bytes, relaunch x2") {
	startDefault();
	auto before = snap(p1);
	openLayoutEditor(); removeBtn(uiSet, Mode); closeLayoutEditor();
	bit7OnRemainingUI("right after save");
	for(int i = 1; i <= 2; i++) {
		relaunch();
		auto w = "after relaunch " + std::to_string(i);
		modeGone(w); bit7OnRemainingUI(w); sameAs(p1, before, w);
	}
}

CASE("p1 B add Mode back with custom pos/hidden, kept after relaunch x2") {
	startDefault();
	openLayoutEditor(); removeBtn(uiSet, Mode); closeLayoutEditor();
	relaunch();
	CHECK(!exists(uiSet, Mode));
	openLayoutEditor();
	addBack(uiSet, Mode);
	Pos want{90, -60};
	moveBtn(uiSet, Mode, want); hideBtn(uiSet, Mode, true);
	closeLayoutEditor();
	relaunch(2);
	CHECK_MSG(exists(uiSet, Mode), "re-added Mode lost");
	if(!exists(uiSet, Mode)) return;
	CHECK_MSG(pos(uiSet, Mode) == want, "Mode position not kept");
	CHECK_MSG(hidden(uiSet, Mode), "Mode hidden not kept");
	CHECK_MSG(!(savedDuoFlags(save(), uiSet, OpenMenu) & removedFlag), "bit 7 still set after Mode was added back");
}

CASE("p1 C move/hide P1 Dpad, Start and Mode, kept after relaunch x2") {
	startDefault();
	auto all = snapAll();
	Pos dpadWant{-30, 40}, startWant{5, -5}, modeWant{-15, -12};
	openLayoutEditor();
	moveBtn(p1, Dpad, dpadWant);
	moveBtn(p1, Start, startWant); hideBtn(p1, Start, true);
	moveBtn(uiSet, Mode, modeWant); hideBtn(uiSet, Mode, true);
	closeLayoutEditor();
	for(int i = 1; i <= 2; i++) {
		relaunch();
		auto w = " after relaunch " + std::to_string(i);
		CHECK_MSG(exists(p1, Dpad) && pos(p1, Dpad) == dpadWant, "P1 Dpad move lost" + w);
		CHECK_MSG(exists(p1, Start) && pos(p1, Start) == startWant, "P1 Start move lost" + w);
		CHECK_MSG(exists(p1, Start) && hidden(p1, Start), "P1 Start hide lost" + w);
		CHECK_MSG(exists(uiSet, Mode) && pos(uiSet, Mode) == modeWant, "Mode move lost" + w);
		CHECK_MSG(exists(uiSet, Mode) && hidden(uiSet, Mode), "Mode hide lost" + w);
		CHECK_MSG(savedDuoRole(save(), p1, Dpad) == p1, "P1 Dpad duoRole not 0" + w);
	}
	othersIntact(all, "after P1 move/hide");
}

CASE("p1 D Reset UI Groups restores Mode at defaults, clears removed mark") {
	startDefault();
	auto all = snapAll();
	std::map<int, Pos> uiPos; std::map<int, bool> uiHid;
	for(int k : defaultUIKeys()) { uiPos[k] = pos(uiSet, k); uiHid[k] = hidden(uiSet, k); }
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	moveBtn(uiSet, OpenMenu, {111, 222}); hideBtn(uiSet, OpenMenu, true);
	removeBtn(p1, Y);
	closeLayoutEditor();
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "precondition: Mode should be gone before reset");
	resetUIGroups();
	closeLayoutEditor();
	for(int i = 0; i <= 2; i++) {
		auto w = i ? " after relaunch " + std::to_string(i) : std::string(" right after reset");
		auto cfg = save();
		for(int k : defaultUIKeys()) {
			CHECK_MSG(exists(uiSet, k), std::string("UI ") + keyName(k) + " missing" + w);
			if(!exists(uiSet, k)) continue;
			CHECK_MSG(pos(uiSet, k) == uiPos[k], std::string("UI ") + keyName(k) + " not at default pos" + w);
			CHECK_MSG(hidden(uiSet, k) == uiHid[k], std::string("UI ") + keyName(k) + " hidden not default" + w);
			if(k != Mode) CHECK_MSG(!(savedDuoFlags(cfg, uiSet, k) & removedFlag), std::string("bit 7 still on ") + keyName(k) + w);
		}
		// Reset UI Groups touches only UI: deleted P1 Y stays deleted, other sets unchanged
		CHECK_MSG(!exists(p1, Y), "P1 Y came back from Reset UI Groups" + w);
		othersIntact(all, "D" + w);
		if(i < 2) relaunch();
	}
}

CASE("p1 E old NEO-Duo 2P config loads P1 (role 0) and Mode; P3/P4 default") {
	loadConfig(neoDuo2PlayerConfig());
	CHECK_MSG(exists(uiSet, Mode), "Mode missing after loading 2P config");
	for(int k : gameKeys()) CHECK_MSG(exists(p1, k), std::string("P1 missing ") + keyName(k));
	CHECK_MSG(savedDuoRole(save(), p1, A) == p1, "P1 duoRole is not 0");
	for(int s : {2, 3}) CHECK_MSG(count(s) == (int)gameKeys().size(), "P" + std::to_string(s + 1) + " no defaults");
	auto before = snap(p1);
	relaunch(2);
	sameAs(p1, before, "after 2P config + relaunch x2");
}

CASE("p1 F close editor saves; kill keeps Mode removal, P1 move and P1 delete") {
	startDefault();
	auto before = snap(p1);
	Pos want{before.p[A].x + 12, before.p[A].y - 6};
	openLayoutEditor();
	removeBtn(uiSet, Mode); moveBtn(p1, A, want); removeBtn(p1, Z);
	closeLayoutEditor();
	killAfterEditor();
	CHECK_MSG(!exists(uiSet, Mode), "Mode back after kill");
	CHECK_MSG(exists(p1, A) && pos(p1, A) == want, "P1 A move lost after kill");
	CHECK_MSG(!exists(p1, Z), "deleted P1 Z back after kill");
	relaunch();
	CHECK(!exists(uiSet, Mode)); CHECK(!exists(p1, Z));
}

CASE("p1 G remove Mode, switch layout + players 4/2/1/4, relaunch x2, stays gone") {
	startDefault();
	auto before = snap(p1);
	openLayoutEditor(); removeBtn(uiSet, Mode); closeLayoutEditor();
	switchLayout(); CHECK_MSG(!exists(uiSet, Mode), "Mode back right after switchLayout");
	for(int n : {2, 1, 4}) { setPlayers(n); CHECK_MSG(!exists(uiSet, Mode), "Mode back after setPlayers(" + std::to_string(n) + ")"); }
	relaunch();
	modeGone("after relaunch 1");
	switchLayout();
	relaunch();
	modeGone("after switch back + relaunch 2");
	sameAs(p1, before, "after G");
}

CASE("p1 G2 default UI: bit7 survives every switch/player change and relaunch x2") {
	startDefault();
	openLayoutEditor(); removeBtn(uiSet, Mode); closeLayoutEditor();
	switchLayout(); bit7OnRemainingUI("after switchLayout");
	for(int n : {2, 1, 4}) { setPlayers(n); bit7OnRemainingUI("after setPlayers(" + std::to_string(n) + ")"); }
	for(int i = 1; i <= 2; i++) {
		relaunch(); auto w = "after relaunch " + std::to_string(i);
		modeGone(w); bit7OnRemainingUI(w); switchLayout();
	}
}

CASE("p1 H remove all UI, relaunch x2, no crash/no regrow, P1 intact") {
	startDefault();
	auto before = snap(p1);
	openLayoutEditor(); removeAllUI(); closeLayoutEditor();
	for(int i = 1; i <= 2; i++) {
		relaunch(); auto w = "after relaunch " + std::to_string(i);
		CHECK_MSG(count(uiSet) == 0, "UI reappeared " + w);
		sameAs(p1, before, w);
	}
}

CASE("p1 H-pad remove every P1 game key, relaunch x2, set stays empty, others intact") {
	startDefault();
	auto all = snapAll();
	openLayoutEditor(); for(int k : gameKeys()) removeBtn(p1, k); closeLayoutEditor();
	CHECK(count(p1) == 0);
	for(int i = 1; i <= 2; i++) {
		relaunch(); auto w = "after relaunch " + std::to_string(i);
		CHECK_MSG(count(p1) == 0, "P1 buttons regrew " + w + " (empty set treated as old config?)");
		othersIntact(all, w);
		for(int k : defaultUIKeys()) CHECK_MSG(exists(uiSet, k), std::string("UI ") + keyName(k) + " changed " + w);
	}
}

CASE("p1 I remove all UI, add back non-Mode, relaunch x2, Mode stays gone") {
	startDefault();
	openLayoutEditor(); removeAllUI(); closeLayoutEditor();
	relaunch();
	openLayoutEditor(); addBack(uiSet, FastForward); closeLayoutEditor();
	relaunch();
	CHECK_MSG(exists(uiSet, FastForward), "re-added FastForward lost");
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back after relaunch 1");
	relaunch();
	CHECK_MSG(!exists(uiSet, Mode), "Mode came back after relaunch 2");
}
