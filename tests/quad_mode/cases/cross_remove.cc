// West (ตก) 5 -- cross-player case 3: removing a button in one set (or the shared Mode button)
// leaves every OTHER set identical, in memory and in the saved bytes, also after relaunch x2.
// "Saved bytes" are read per (set,key) through savedDuoFlags/savedDuoRole on the SAVED config, not memory.
#include "quad_harness.hh"
#include <map>
#include <tuple>

namespace cross_remove {
using namespace quad;

static void requireReal() { std::string why; if(!backendReady(why)) PENDING(why); }

struct BtnSnap { bool ex, hid; Pos p; uint8_t flags, role; bool operator==(const BtnSnap &) const = default; };
using SetSnap = std::map<int, BtnSnap>;

static SetSnap snap(int s, const std::vector<uint8_t> &cfg)
{
	SetSnap m;
	for(int k : gameKeys())
	{
		bool ex = exists(s, k);
		m[k] = {ex, ex && hidden(s, k), ex ? pos(s, k) : Pos{}, ex ? savedDuoFlags(cfg, s, k) : uint8_t(0),
			ex ? savedDuoRole(cfg, s, k) : uint8_t(0)};
	}
	return m;
}

static void expectSame(int s, const SetSnap &before, const std::vector<uint8_t> &cfg, const std::string &when)
{
	SetSnap now = snap(s, cfg);
	for(auto &[k, b] : before)
		CHECK_MSG(now[k] == b, when + ": P" + std::to_string(s + 1) + " " + keyName(k) + " changed");
}

static void runRemove(int victimSet, int key)
{
	freshInstall();
	setPlayers(4);
	auto cfg0 = save();
	std::array<SetSnap, numSets> before;
	FOR_SETS(s) before[s] = snap(s, cfg0);
	std::string tag = std::string("remove ") + (victimSet == uiSet ? "UI" : "P" + std::to_string(victimSet + 1)) + " " + keyName(key);
	openLayoutEditor();
	removeBtn(victimSet, key);
	closeLayoutEditor();
	auto cfg1 = save();
	FOR_SETS(s) if(s != victimSet) expectSame(s, before[s], cfg1, tag + " (memory+saved)");
	for(int r = 1; r <= 2; ++r)
	{
		relaunch();
		auto cfg = save();
		CHECK_MSG(!exists(victimSet, key), tag + ": came back after relaunch " + std::to_string(r));
		FOR_SETS(s) if(s != victimSet) expectSame(s, before[s], cfg, tag + " relaunch " + std::to_string(r));
	}
}

CASE("cross 3 remove game button in P1..P4: other sets unchanged (mem+saved, relaunch x2)")
{
	requireReal();
	FOR_SETS(v) for(int k : {(int)A, (int)Start, (int)Dpad}) runRemove(v, k);
}

CASE("cross 3b remove shared Mode (three-line) button: all 4 player sets unchanged, Mode stays gone")
{
	requireReal();
	runRemove(uiSet, Mode);
	auto cfg = save();
	int withBit7 = 0;
	for(int k : defaultUIKeys())
		if(k != Mode && exists(uiSet, k) && (savedDuoFlags(cfg, uiSet, k) & removedFlag)) ++withBit7;
	CHECK_MSG(withBit7 > 0, "no remaining UI button carries duoFlags bit7 after removing Mode");
}

CASE("cross 3c remove all of P2's buttons: P1/P3/P4 still complete and press-isolated")
{
	requireReal();
	freshInstall();
	setPlayers(4);
	openLayoutEditor();
	for(int k : gameKeys()) removeBtn(1, k);
	closeLayoutEditor();
	relaunch(2);
	CHECK(count(1) == 0);
	for(int s : {0, 2, 3})
	{
		CHECK(count(s) == (int)gameKeys().size());
		PortInputs p = press(s, A);
		for(int o = 0; o < numSets; ++o) CHECK(o == s ? p[o] != 0 : p[o] == 0);
	}
}

} // namespace cross_remove
