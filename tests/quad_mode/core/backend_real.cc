// REAL backend: glue to extracted EmuFramework/Saturn.emu code (gen/real_code.inc, gen/features.h from extract.py).
// Until the 4-player implementation lands (HAVE_QUAD_IMPL), every case is PENDING -- never a fake pass.
#include "quad_harness.hh"
#include "gen/features.h"
#ifndef HAVE_QUAD_IMPL
namespace quad {
const char *backendName() { return "real"; }
bool backendReady(std::string &why) {
	why = "4-player implementation not found in tree: " QUAD_MISSING; return false;
}
#define NA { throw Abort{Result::Pending, "no implementation"}; }
void freshInstall() NA void freshInstallModeOnlyUI() NA std::vector<uint8_t> save() NA void relaunch() NA
void loadConfig(const std::vector<uint8_t> &) NA void openLayoutEditor() NA void closeLayoutEditor() NA
void killAfterEditor() NA bool exists(int, int) NA bool hidden(int, int) NA Pos pos(int, int) NA int count(int) NA
void removeBtn(int, int) NA void removeAllUI() NA void addBack(int, int) NA void moveBtn(int, int, Pos) NA
void hideBtn(int, int, bool) NA PortInputs press(int, int) NA Rgb color(int) NA void switchLayout() NA
void setPlayers(int) NA int players() NA void setInputPlayer(int) NA int inputPlayer() NA uint8_t savedDuoFlags(const std::vector<uint8_t> &, int, int) NA
uint8_t savedDuoRole(const std::vector<uint8_t> &, int, int) NA std::vector<uint8_t> neoDuo2PlayerConfig() NA
void resetUIGroups() NA bool multitapEnabled(int) NA void setPadSize(int, int) NA int padSize(int) NA
void setPadAlpha(int, int) NA int padAlpha(int) NA PortInputs pressTurbo(int, int) NA
bool modeHoldShouldToggle(bool, bool) NA
bool modePressRemainsPending(bool, bool, bool) NA
bool midGameOverlayTap(bool, bool, bool, bool) NA
bool midGameOverlayDeferred(bool, bool, bool, bool) NA
int overlayForcedPadEnd(int, int, int) NA
}
#else
#include "backend_real_impl.inc" // written by west when the PR lands (maps helpers -> real VController API)
#endif
