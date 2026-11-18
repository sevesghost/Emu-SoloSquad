// MODEL backend: executable reference of the 4-player SPEC, used only to validate the cases
// (QUAD_BACKEND=model). Implements the NEO-Duo f81076b rules generalized to 4 roles:
//  - removed Mode = duoFlags bit7 on remaining UI elements; empty UI layout saved as a placeholder
//    at (-2,-2) carrying the flag; syncDuoModeButton() re-adds Mode only if no bit7 is saved
//  - closing the layout editor saves; per-set positions/hidden/removed are independent
// QUAD_MODEL_BUG injects a bug so we can prove the cases catch it.
#include "quad_harness.hh"
#include <cstdlib>
#include <cstring>
#include <map>
namespace quad {
namespace {
struct Btn { bool present = true, hidden = false; Pos p; };
struct App {
	std::map<int, Btn> set[numSets]; std::map<int, Btn> ui;
	bool modeRemoved = false, editor = false, joint = false; int nPlayers = 4;
	int size[numSets]{1000, 1000, 1000, 1000}, alpha[numSets]{127, 127, 127, 127};
	std::vector<uint8_t> disk; // what saveConfigFile wrote
} app;
const char *bug() { const char *b = getenv("QUAD_MODEL_BUG"); return b ? b : ""; }
bool hasBug(const char *b) { return !strcmp(bug(), b); }
Btn *find(int s, int k) {
	auto &m = s == uiSet ? app.ui : app.set[s];
	auto it = m.find(k); return it == m.end() ? nullptr : &it->second;
}
Pos defPos(int s, int k) { return {s * 1000 + k * 10, s * 500 + k}; }
void defaults(int s) { app.set[s].clear(); for(int k : gameKeys()) app.set[s][k] = {true, false, defPos(s, k)}; }
void syncDuoModeButton() {
	if(!app.modeRemoved && !find(uiSet, Mode)) app.ui[Mode] = {true, false, {-1, -1}};
}
void put8(std::vector<uint8_t> &v, int x) { v.push_back(uint8_t(x)); }
void put16(std::vector<uint8_t> &v, int x) { v.push_back(uint8_t(x)); v.push_back(uint8_t(x >> 8)); }
int get16(const uint8_t *p) { return int16_t(p[0] | p[1] << 8); }
// file: 0x02 | u16 len | u16 key(113) | u16 n | n * {u8 set(0xFF=ui) u8 key i16 x i16 y u8 hidden}
//       | n * trailer {i16 sizeMM100x u8 ownAlpha u8 duoFlags u8 duoRole}   (5 bytes/elem)
struct Elem { int set, key; Pos p; bool hidden; uint8_t flags, role; int size = 1000, alpha = 255; };
std::vector<uint8_t> encode(const std::vector<Elem> &es) {
	std::vector<uint8_t> b; put16(b, 113); put16(b, es.size());
	for(auto &e : es) { put8(b, e.set == uiSet ? 0xFF : e.set); put8(b, e.key); put16(b, e.p.x); put16(b, e.p.y); put8(b, e.hidden); }
	for(auto &e : es) { put16(b, e.size); put8(b, e.alpha); put8(b, e.flags); put8(b, e.role); }
	std::vector<uint8_t> f{2}; put16(f, b.size()); f.insert(f.end(), b.begin(), b.end()); return f;
}
std::vector<Elem> decode(const std::vector<uint8_t> &f) {
	CHECK_MSG(f.size() >= 1 && f[0] == 2, "bad config header");
	// full config file: 0x02 | blocks { u16 size (incl. 2-byte key) | u16 key | payload }
	const uint8_t *blk = nullptr; size_t len = 0;
	for(size_t o = 1; o < f.size();) {
		CHECK_MSG(o + 4 <= f.size(), "truncated block header");
		size_t sz = f[o] | f[o + 1] << 8; CHECK_MSG(sz >= 2 && o + 2 + sz <= f.size(), "block size past end of file");
		if(get16(&f[o + 2]) == 113) { blk = &f[o + 2]; len = sz; }
		o += 2 + sz;
	}
	CHECK_MSG(blk, "no key 113 block");
	const uint8_t *p = blk; int n = get16(p + 2); p += 4;
	if(len != 4 + size_t(n) * 12) throw Abort{Result::Skip, "key 113 is real NEO-Duo element format (not model format): real backend only"};
	std::vector<Elem> es(n);
	for(auto &e : es) { e.set = p[0] == 0xFF ? uiSet : p[0]; e.key = p[1]; e.p = {get16(p + 2), get16(p + 4)}; e.hidden = p[6]; p += 7; }
	for(auto &e : es) { e.size = get16(p); e.alpha = p[2]; e.flags = p[3]; e.role = p[4]; p += 5; }
	return es;
}
uint8_t roleOf(int s) { return s == uiSet ? 0 : s; }
// players() persistence (west 5, lead-approved 21:26): extra block {u16 size=3, u16 key 0x7F01, u8 nPlayers}
// appended by save(); files without it (old NEO-Duo / pre-change) keep the default (4).
constexpr int playersKey = 0x7F01;
int savedPlayers(const std::vector<uint8_t> &f) {
	for(size_t o = 1; o + 4 <= f.size();) {
		size_t sz = f[o] | f[o + 1] << 8; if(sz < 2 || o + 2 + sz > f.size()) break;
		if(get16(&f[o + 2]) == playersKey && sz == 3) return f[o + 4];
		o += 2 + sz;
	}
	return -1;
}
}
const char *backendName() { return "model"; }
bool backendReady(std::string &) { return true; }
static void freshUI() { app.ui[OpenMenu] = {true, false, {9000, 0}}; app.ui[FastForward] = {true, false, {0, 0}}; app.ui[Rewind] = {true, false, {100, 0}}; }
void freshInstall() {
	app = {}; for(int s = 0; s < numSets; ++s) defaults(s);
	freshUI();
	syncDuoModeButton();
}
void freshInstallModeOnlyUI() { freshInstall(); app.ui.clear(); syncDuoModeButton(); }
std::vector<uint8_t> save() {
	std::vector<Elem> es;
	bool mr = app.modeRemoved && !hasBug("drop_bit7");
	for(int s = 0; s < numSets; ++s) {
		uint8_t fl = uint8_t(playerSetField(s) << playerSetShift); bool any = false;
		int sz = app.size[hasBug("size_clobber") && s >= 2 ? 1 : s], al = app.alpha[s];
		for(auto &[k, b] : app.set[s]) if(b.present) { any = true;
			es.push_back({s, k, hasBug("lose_move") ? defPos(s, k) : b.p, b.hidden && !hasBug("lose_hide"), fl, roleOf(s), sz, al}); }
		if(!any && !hasBug("no_set_marker")) es.push_back({s, 0, {-2, -2}, false, uint8_t(fl | removedFlag), roleOf(s), sz, al}); // user removed whole set
	}
	bool anyUI = false;
	for(auto &[k, b] : app.ui) if(b.present) { anyUI = true; es.push_back({uiSet, k, b.p, b.hidden, uint8_t(mr ? removedFlag : 0), 0}); }
	if(!anyUI) es.push_back({uiSet, 0, {-2, -2}, false, uint8_t(mr ? removedFlag : 0), 0}); // empty-layout placeholder
	app.disk = encode(es); put16(app.disk, 3); put16(app.disk, playersKey); put8(app.disk, app.nPlayers); return app.disk;
}
void loadConfig(const std::vector<uint8_t> &bytes) {
	auto es = decode(bytes); app = {}; app.disk = bytes;
	bool seen[numSets]{};
	for(auto &e : es) {
		if(e.set == uiSet) {
			if(e.flags & removedFlag) app.modeRemoved = true;
			if(e.key) app.ui[e.key] = {true, e.hidden, e.p};
		} else { CHECK_MSG(e.set < numSets && e.role == e.set, "role/set mismatch"); seen[e.set] = true;
			app.size[e.set] = e.size; app.alpha[e.set] = e.alpha; if(e.key) app.set[e.set][e.key] = {true, e.hidden, e.p}; }
	}
	for(int s = 0; s < numSets; ++s) if(!seen[s] && !hasBug("old_cfg")) defaults(s); // old 2P config: roles 2..3 default
	for(int s = 0; s < numSets; ++s) for(int k : gameKeys()) if(!find(s, k)) app.set[s][k] = {false, false, defPos(s, k)};
	if(int n = savedPlayers(bytes); n > 0 && !hasBug("forget_players")) app.nPlayers = n;
	else if(n <= 0) app.nPlayers = hasBug("old_cfg_4p") ? 4 : 2; // no player block: old config stays 2P
	if(hasBug("resurrect_mode")) app.modeRemoved = false;
	syncDuoModeButton();
}
void relaunch() { loadConfig(save()); }
void openLayoutEditor() { app.editor = true; }
void closeLayoutEditor() { app.editor = false; save(); }
void killAfterEditor() { auto d = app.disk; loadConfig(d); }
bool exists(int s, int k) { auto b = find(s, k); return b && b->present; }
bool hidden(int s, int k) { auto b = find(s, k); return b && b->hidden; }
Pos pos(int s, int k) { auto b = find(s, k); CHECK_MSG(b, std::string("no button ") + keyName(k)); return b->p; }
int count(int s) { int n = 0; for(auto &[k, b] : (s == uiSet ? app.ui : app.set[s])) n += b.present; return n; }
void removeBtn(int s, int k) {
	auto b = find(s, k); CHECK_MSG(b && b->present, std::string("remove: no ") + keyName(k));
	if(s == uiSet) { app.ui.erase(k); if(k == Mode) app.modeRemoved = true; } else b->present = false;
}
void removeAllUI() { for(auto k : defaultUIKeys()) if(exists(uiSet, k)) removeBtn(uiSet, k); }
void addBack(int s, int k) {
	if(s == uiSet) { app.ui[k] = {true, false, {500, 500}}; if(k == Mode) app.modeRemoved = false; }
	else { auto b = find(s, k); CHECK(b); b->present = true; }
}
void moveBtn(int s, int k, Pos p) { auto b = find(s, k); CHECK(b); b->p = p; }
void hideBtn(int s, int k, bool h) { auto b = find(s, k); CHECK(b); b->hidden = h; }
PortInputs press(int s, int k) {
	PortInputs r{}; if(s == uiSet || s >= app.nPlayers) return r;
	auto b = find(s, k); if(!b || !b->present || b->hidden) return r;
	r[hasBug("cross_route") && s == 3 ? 2 : s] |= 1u << k; return r;
}
Rgb color(int s) { static const Rgb c[4]{{230, 60, 60}, {60, 120, 230}, {60, 200, 90}, {240, 200, 40}}; return c[s]; }
void switchLayout() { app.joint = !app.joint; }
void setPlayers(int n) { app.nPlayers = n; if(hasBug("mode_switch_clears_bit7")) app.modeRemoved = false; syncDuoModeButton(); }
int players() { return app.nPlayers; }
static const Elem *findSaved(const std::vector<Elem> &es, int s, int k) {
	for(auto &e : es) if(e.set == s && e.key == k) return &e;
	if(s == uiSet) for(auto &e : es) if(e.set == uiSet) return &e; // any UI carrier/placeholder
	return nullptr;
}
uint8_t savedDuoFlags(const std::vector<uint8_t> &cfg, int s, int k) { auto es = decode(cfg); auto e = findSaved(es, s, k); return e ? e->flags : 0; }
uint8_t savedDuoRole(const std::vector<uint8_t> &cfg, int s, int k) { auto es = decode(cfg); auto e = findSaved(es, s, k); return e ? e->role : 0xFF; }
void resetUIGroups() { app.ui.clear(); app.modeRemoved = false; freshUI(); syncDuoModeButton(); }
bool multitapEnabled(int port) { return port == 0 && app.nPlayers > 2 && !hasBug("no_multitap"); }
void setPadSize(int s, int v) { app.size[s] = v; } int padSize(int s) { return app.size[s]; }
void setPadAlpha(int s, int v) { app.alpha[s] = v; } int padAlpha(int s) { return app.alpha[s]; }
PortInputs pressTurbo(int s, int k) { if(hasBug("turbo_2p") && s >= 2) return {}; return press(s, k); }
std::vector<uint8_t> neoDuo2PlayerConfig() {
	std::vector<Elem> es;
	for(int s = 0; s < 2; ++s) for(int k : gameKeys()) es.push_back({s, k, defPos(s, k), false, 0, uint8_t(s)});
	es.push_back({uiSet, OpenMenu, {9000, 0}, false, 0, 0}); es.push_back({uiSet, Mode, {-1, -1}, false, 0, 0});
	return encode(es);
}
}
