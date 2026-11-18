// West (ตก) 5 -- cross-player case 4: a NEO-Duo 2-player config loads in quad mode and maps P1/P2.
// 4a uses the harness's NEO-Duo v1.1 blob; 4b loads the REAL sample files in tools/samples-pr2b/
// (current duoFlags bit-7 format, lead's decision; samples-pr2 = old 859feff format is NOT used).
// Dir: $QUAD_NEO_SAMPLES (run.sh exports tools/neoduo-samples/samples-pr2b), same default here. Missing -> SKIP.
#include "quad_harness.hh"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace cross_neoduo_cfg {
using namespace quad;
namespace fs = std::filesystem;

static void requireReal() { std::string why; if(!backendReady(why)) PENDING(why); }

static void checkP1P2(const std::string &when)
{
	for(int s : {0, 1})
		for(int k : gameKeys())
		{
			CHECK_MSG(exists(s, k), when + ": P" + std::to_string(s + 1) + " " + keyName(k) + " missing");
			PortInputs p = press(s, k);
			for(int o = 0; o < numSets; ++o)
				CHECK_MSG(o == s ? p[o] != 0 : p[o] == 0, when + ": P" + std::to_string(s + 1) + " " + keyName(k) + " wrong port " + std::to_string(o + 1));
		}
	auto cfg = save();
	for(int k : gameKeys())
	{
		CHECK_MSG(savedDuoRole(cfg, 0, k) == 0, when + ": P1 role not 0 for " + keyName(k));
		CHECK_MSG(savedDuoRole(cfg, 1, k) == 1, when + ": P2 role not 1 for " + keyName(k));
	}
}

CASE("cross 4 NEO-Duo 2p config loads in quad mode, P1/P2 mapped to ports 1/2")
{
	requireReal();
	loadConfig(neoDuo2PlayerConfig());
	checkP1P2("loaded");
	for(int s : {2, 3}) // roles 2..3 absent from a 2P file -> must get the default buttons
		CHECK_MSG(count(s) == (int)gameKeys().size(), "P" + std::to_string(s + 1) + " has no default buttons after loading 2P config");
	setPlayers(4);
	checkP1P2("after switching to 4p");
	relaunch(2);
	checkP1P2("after relaunch x2");
}

CASE("cross 4b real NEO-Duo samples-pr2b (bit-7 format) load; Mode presence preserved")
{
	requireReal();
	const char *env = std::getenv("QUAD_NEO_SAMPLES");
	fs::path dir = env ? env : "/workspace/saturn-quad-tests/tests/quad_mode/tools/neoduo-samples/samples-pr2b";
	if(!fs::is_directory(dir)) SKIP("no NEO-Duo samples-pr2b at " + dir.string());
	int loaded = 0;
	for(auto &f : fs::directory_iterator(dir))
	{
		if(f.path().extension() != ".cfg") continue;
		std::ifstream in(f.path(), std::ios::binary);
		std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
		std::string n = f.path().filename().string(), when = "samples-pr2b/" + n;
		loadConfig(bytes);
		++loaded;
		// present.cfg / moved-hidden.cfg keep Mode; removed.cfg has Mode removed (bit 7 on other UI buttons)
		bool removed = n.find("removed") != std::string::npos;
		if(removed) CHECK_MSG(!exists(uiSet, Mode), when + ": Mode came back");
		else CHECK_MSG(exists(uiSet, Mode), when + ": Mode lost");
		for(int s : {0, 1})
			CHECK_MSG(count(s) == (int)gameKeys().size(), when + ": P" + std::to_string(s + 1) + " incomplete");
		for(int s : {2, 3})
			CHECK_MSG(count(s) == (int)gameKeys().size(), when + ": P" + std::to_string(s + 1) + " has no default buttons");
		relaunch(2);
		if(removed) CHECK_MSG(!exists(uiSet, Mode), when + ": Mode came back after relaunch x2");
		else CHECK_MSG(exists(uiSet, Mode), when + ": Mode lost after relaunch x2");
	}
	CHECK_MSG(loaded == 3, "expected 3 .cfg in " + dir.string() + ", loaded " + std::to_string(loaded));
}

} // namespace cross_neoduo_cfg
