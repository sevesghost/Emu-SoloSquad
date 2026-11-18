#include "quad_harness.hh"
#include <cstring>
#include <cstdlib>
namespace quad {
std::vector<Case> &registry() { static std::vector<Case> r; return r; }
static std::string curFail;
void check(bool ok, const char *expr, const char *file, int line, const std::string &msg) {
	if(ok) return;
	char b[512]; snprintf(b, sizeof b, "%s:%d: CHECK(%s) failed%s%s", file, line, expr, msg.empty() ? "" : ": ", msg.c_str());
	throw Abort{Result::Fail, b};
}
const char *keyName(int k) {
	switch(k) {
		case Dpad: return "Dpad"; case A: return "A"; case B: return "B"; case C: return "C";
		case X: return "X"; case Y: return "Y"; case Z: return "Z"; case L: return "L"; case R: return "R";
		case Start: return "Start"; case OpenMenu: return "OpenMenu"; case FastForward: return "FastForward";
		case Rewind: return "Rewind"; case Mode: return "Mode";
	}
	return "?";
}
std::vector<int> gameKeys() { return {Dpad, A, B, C, X, Y, Z, L, R, Start}; }
std::vector<int> defaultUIKeys() { return {OpenMenu, FastForward, Rewind, Mode}; }
void relaunch(int times) { for(int i = 0; i < times; ++i) relaunch(); }
}
using namespace quad;
int main(int argc, char **argv) {
	const char *filter = argc > 1 ? argv[1] : nullptr;
	std::string why; bool ready = backendReady(why);
	printf("backend: %s%s%s\n", backendName(), ready ? "" : " (NOT READY: ", ready ? "" : (why + ")").c_str());
	int n[6] = {};
	const char *lbl[] = {"PASS", "FAIL", "SKIP", "PENDING", "XFAIL", "XPASS"};
	bool model = !strcmp(backendName(), "model");
	for(auto &c : registry()) {
		if(filter && c.name.find(filter) == std::string::npos) continue;
		Result r = Result::Pass; std::string msg;
		if(!ready) { r = Result::Pending; msg = why; }
		else try { c.fn(); } catch(Abort &a) { r = a.r; msg = a.why; } catch(std::exception &e) { r = Result::Fail; msg = e.what(); }
		if(c.knownIssue) { if(r == Result::Fail) r = Result::XFail; else if(r == Result::Pass) r = Result::XPass; }
		n[(int)r]++;
		printf("%s%-7s %s%s%s\n", model ? "MODEL " : "", lbl[(int)r], c.name.c_str(), msg.empty() ? "" : "  -- ", msg.c_str());
	}
	printf("%ssummary: %d pass, %d fail, %d skip, %d pending; known issues (not counted): %d xfail, %d xpass\n",
		model ? "MODEL (spec self-check, NOT the implementation) " : "", n[0], n[1], n[2], n[3], n[4], n[5]);
	if(n[1]) return 1;
	if(getenv("QUAD_STRICT") && n[3]) return 3; // CI: pending counts as not done
	return 0;
}
