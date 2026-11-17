// West (ตก) 5 -- cross-player case 2: switching player modes / layouts back and forth
// leaves no overlapping and no missing buttons for any player.
// Limitation: the harness exposes only Pos (no size), so "overlap" = two visible buttons at the same Pos.
#include "quad_harness.hh"
#include <set>
#include <utility>

namespace cross_modes {
using namespace quad;

static void requireReal() { std::string why; if(!backendReady(why)) PENDING(why); }

static void checkLayout(const std::string &when, bool expectMode = true)
{
	int n = players();
	CHECK_MSG(n == 1 || n == 2 || n == 4, when + ": unexpected players()=" + std::to_string(n));
	std::set<std::pair<int,int>> used;
	auto claim = [&](int s, int k)
	{
		if(hidden(s, k)) return;
		Pos p = pos(s, k);
		bool fresh = used.insert({p.x, p.y}).second;
		CHECK_MSG(fresh, when + ": set " + std::to_string(s) + " " + keyName(k) + " overlaps another button at "
			+ std::to_string(p.x) + "," + std::to_string(p.y));
	};
	for(int s = 0; s < n; ++s)
	{
		for(int k : gameKeys())
		{
			CHECK_MSG(exists(s, k), when + ": P" + std::to_string(s + 1) + " " + keyName(k) + " missing");
			if(exists(s, k)) claim(s, k);
		}
		CHECK_MSG(count(s) == (int)gameKeys().size(), when + ": P" + std::to_string(s + 1) + " count=" + std::to_string(count(s)));
	}
	for(int k : defaultUIKeys())
		if(exists(uiSet, k)) claim(uiSet, k);
	if(expectMode) CHECK_MSG(exists(uiSet, Mode), when + ": Mode button missing (was never removed)");
}

CASE("cross 2 cycle 4p/2p/1p modes x3: no overlap, no missing button")
{
	requireReal();
	freshInstall();
	setPlayers(4);
	checkLayout("fresh 4p");
	const int seq[] = {2, 1, 4, 1, 2, 4};
	for(int cycle = 0; cycle < 3; ++cycle)
		for(int n : seq)
		{
			setPlayers(n);
			checkLayout("cycle " + std::to_string(cycle) + " -> " + std::to_string(n) + "p");
		}
	relaunch(2);
	checkLayout("after relaunch x2");
}

CASE("cross 2b Joint/Separate switch x6 in each mode: no overlap, no missing")
{
	requireReal();
	freshInstall();
	for(int n : {4, 2, 1})
	{
		setPlayers(n);
		for(int i = 0; i < 6; ++i)
		{
			switchLayout();
			checkLayout(std::to_string(n) + "p switch " + std::to_string(i + 1));
		}
		relaunch();
		checkLayout(std::to_string(n) + "p after relaunch");
	}
}

CASE("cross 2c moved/hidden button keeps its value across mode cycling")
{
	requireReal();
	freshInstall();
	setPlayers(4);
	Pos p{123, 77};
	moveBtn(2, B, p);
	hideBtn(3, Start, true);
	for(int n : {2, 1, 4, 2, 4}) setPlayers(n);
	relaunch(2);
	CHECK(players() == 4);
	CHECK(pos(2, B) == p);
	CHECK(hidden(3, Start));
	CHECK(!hidden(0, Start) && !hidden(1, Start) && !hidden(2, Start));
}

// Mode removed (real remove path) must stay removed through every player-count switch:
// Mode absent and bit 7 on the remaining UI buttons, read from the SAVED key-113 bytes.
static void checkModeRemoved(const std::string &when)
{
	CHECK_MSG(!exists(uiSet, Mode), when + ": Mode came back");
	auto cfg = save();
	int ui = 0;
	for(int k : defaultUIKeys())
	{
		if(k == Mode || !exists(uiSet, k)) continue;
		++ui;
		CHECK_MSG(savedDuoFlags(cfg, uiSet, k) & removedFlag, when + ": saved " + keyName(k) + " lost duoFlags bit7");
	}
	CHECK_MSG(ui > 0, when + ": no UI button left to carry bit7");
}

CASE("cross 2d Mode removed, cycle 4p/2p/1p x3 + relaunch x2: Mode stays gone, saved bit7 kept, no overlap/missing")
{
	requireReal();
	freshInstall();
	setPlayers(4);
	openLayoutEditor();
	removeBtn(uiSet, Mode);
	closeLayoutEditor();
	checkModeRemoved("after remove");
	const int seq[] = {2, 1, 4, 1, 2, 4};
	for(int cycle = 0; cycle < 3; ++cycle)
		for(int n : seq)
		{
			std::string when = "cycle " + std::to_string(cycle) + " -> " + std::to_string(n) + "p";
			setPlayers(n);
			checkLayout(when, false);
			checkModeRemoved(when);
			for(int r = 1; r <= 2; ++r)
			{
				relaunch();
				std::string w = when + " relaunch " + std::to_string(r);
				CHECK_MSG(players() == n, w + ": players() not kept across relaunch (got " + std::to_string(players()) + ")");
				checkLayout(w, false);
				checkModeRemoved(w);
			}
		}
}

} // namespace cross_modes
