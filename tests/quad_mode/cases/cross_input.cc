// West (ตก) 5 -- cross-player case 1: a touch on player N's button reaches ONLY Saturn port N.
// Starts from the real default 4-set layout (freshInstall), never the Mode-only layout.
// Cases sit in their own namespace so CASE()'s __LINE__-based names can't clash with other files.
#include "quad_harness.hh"

namespace cross_input {
using namespace quad;

static void requireReal() { std::string why; if(!backendReady(why)) PENDING(why); }

static void checkIsolation(const char *when)
{
	FOR_SETS(s)
	{
		for(int k : gameKeys())
		{
			CHECK_MSG(exists(s, k), std::string(when) + ": P" + std::to_string(s + 1) + " " + keyName(k) + " missing");
			PortInputs p = press(s, k);
			CHECK_MSG(p[s] != 0, std::string(when) + ": P" + std::to_string(s + 1) + " " + keyName(k) + " produced no input on its own port");
			for(int o = 0; o < numSets; ++o)
			{
				if(o == s) continue;
				CHECK_MSG(p[o] == 0, std::string(when) + ": P" + std::to_string(s + 1) + " " + keyName(k) + " leaked to port " + std::to_string(o + 1));
			}
		}
	}
}

CASE("cross 1 press P1..P4 button only reaches own port")
{
	requireReal();
	freshInstall();
	setPlayers(4);
	checkIsolation("fresh");
}

CASE("cross 1b press isolation survives relaunch x2 and Joint/Separate switch")
{
	requireReal();
	freshInstall();
	setPlayers(4);
	relaunch(2);
	checkIsolation("after relaunch x2");
	switchLayout();
	checkIsolation("after switchLayout");
	switchLayout();
	relaunch();
	checkIsolation("after switch back + relaunch");
}

CASE("cross 1c UI buttons (Menu/FF/Rewind/Mode) feed no player port")
{
	requireReal();
	freshInstall();
	setPlayers(4);
	for(int k : defaultUIKeys())
	{
		if(!exists(uiSet, k)) continue; // e.g. FF/Rewind only on touch devices
		if(k == Mode) continue;         // Mode changes the layout; covered by cross 2
		PortInputs p = press(uiSet, k);
		FOR_SETS(o) CHECK_MSG(p[o] == 0, std::string("UI ") + keyName(k) + " leaked to port " + std::to_string(o + 1));
	}
}

} // namespace cross_input
