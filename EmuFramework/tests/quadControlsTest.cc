/* Host tests for 4-player on-screen controls.
   g++ -std=c++20 -Wall -Werror -I EmuFramework/include EmuFramework/tests/quadControlsTest.cc -o quadControlsTest && ./quadControlsTest
*/

#include <emuframework/QuadControls.hh>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{

int fails{};

void expect(bool cond, const char *msg)
{
	if(!cond)
	{
		std::cerr << "FAIL " << msg << '\n';
		++fails;
	}
}

using namespace EmuEx;

struct SavedButton
{
	int player{};
	uint8_t state{};
	int16_t x{}, y{};
	uint8_t deviceId{};
	bool emptied{};
	bool hidden{};
};

std::vector<uint8_t> encodeButtons(const std::vector<SavedButton> &buttons, bool modeRemoved, int playerCount)
{
	std::vector<uint8_t> out;
	out.push_back(duoHeaderFlags(playerCount > 1, false, true, modeRemoved));
	auto ext = encodeQuadExtension({uint8_t(playerCount), 1000, 1100, 128, 140});
	out.insert(out.end(), ext.begin(), ext.end());
	out.push_back(uint8_t(buttons.size()));
	for(const auto &b : buttons)
	{
		out.push_back(uint8_t(b.player));
		out.push_back(b.state);
		uint8_t pos[4];
		storeI16LE(pos, b.emptied ? kEmptySetPos : b.x);
		storeI16LE(pos + 2, b.emptied ? kEmptySetPos : b.y);
		out.insert(out.end(), pos, pos + 4);
		out.push_back(b.deviceId);
		uint8_t flags = setFieldForPlayer(b.player) << kDuoPlayerShift;
		if(modeRemoved)
			flags = uint8_t(flags | kDuoModeRemovedFlag);
		out.push_back(flags);
		out.push_back(uint8_t(b.hidden ? 1 : 0));
	}
	return out;
}

struct Loaded
{
	int playerCount{};
	bool modeRemoved{};
	uint8_t present{};
	uint8_t emptied{};
	std::vector<SavedButton> buttons;
};

Loaded decodeButtons(std::span<const uint8_t> bytes)
{
	Loaded loaded;
	expect(!bytes.empty(), "blob has a header");
	if(bytes.empty())
		return loaded;
	size_t i = 0;
	uint8_t flags = bytes[i++];
	loaded.modeRemoved = flags & kDuoFlagModeRemoved;
	expect(bool(flags & kDuoFlagPlayerCount), "player count extension is present");
	expect(i + kQuadExtensionBytes <= bytes.size(), "extension fits");
	if(i + kQuadExtensionBytes > bytes.size())
		return loaded;
	std::array<uint8_t, kQuadExtensionBytes> ext{};
	for(size_t n = 0; n < kQuadExtensionBytes; ++n)
		ext[n] = bytes[i++];
	auto decoded = decodeQuadExtension(ext);
	loaded.playerCount = decoded.playerCount;
	expect(decoded.size3 == 1000 && decoded.size4 == 1100, "p3/p4 sizes round-trip");
	expect(i < bytes.size(), "count byte is present");
	if(i >= bytes.size())
		return loaded;
	uint8_t count = bytes[i++];
	for(uint8_t n = 0; n < count; ++n)
	{
		expect(i + 9 <= bytes.size(), "button record is complete");
		if(i + 9 > bytes.size())
			return loaded;
		SavedButton b;
		b.player = bytes[i++];
		b.state = bytes[i++];
		int16_t x = loadI16LE(&bytes[i]);
		int16_t y = loadI16LE(&bytes[i + 2]);
		i += 4;
		b.deviceId = bytes[i++];
		uint8_t duoFlags = bytes[i++];
		b.hidden = bytes[i++] != 0;
		b.x = x;
		b.y = y;
		b.emptied = isEmptiedSetPos(x, y, x, y);
		expect((duoFlags & kDuoModeRemovedFlag) == 0 || loaded.modeRemoved, "bit 7 stays the global mode flag");
		expect(((duoFlags & kDuoPlayerMask) >> kDuoPlayerShift) <= 7, "player set uses 3 bits");
		expect((duoFlags & kDuoModeRemovedFlag) == kDuoModeRemovedFlag || (duoFlags & 0x80) == 0, "bit 7 is only the mode flag");
		if(b.emptied)
			loaded.emptied = uint8_t(loaded.emptied | uint8_t(1u << b.player));
		else
			loaded.present = uint8_t(loaded.present | uint8_t(1u << b.player));
		int port = playerIndexFromSet((duoFlags & kDuoPlayerMask) >> kDuoPlayerShift);
		if(!b.emptied && port >= 0)
			b.deviceId = uint8_t(port);
		loaded.buttons.push_back(b);
	}
	expect(i == bytes.size(), "save/load consumes every byte");
	return loaded;
}

}

int main()
{
	expect(maxDuoPlayers == 4, "four players is the cap");
	expect(kDuoPlayerMask == (7 << 3), "player set is bits 3-5");
	expect((kDuoPlayerMask & kDuoModeRemovedFlag) == 0, "player bits never include bit 7");
	expect(kDuoModeRemovedFlag == 0x80, "bit 7 remains the mode-removed flag");

	expect(duoRolePort(0) == 0 && duoRolePort(1) == 1 && duoRolePort(2) == 2 && duoRolePort(3) == 3,
		"roles 0..3 are ports 0..3");
	expect(duoRolePort(32) == 1, "legacy P2 block maps to port 1");
	expect(duoRolePort(1) == 1, "role 1 is port 1 in the new encoding");

	auto blue = duoRoleColorRgba(0);
	auto red = duoRoleColorRgba(1);
	auto green = duoRoleColorRgba(2);
	auto amber = duoRoleColorRgba(3);
	expect(blue.a > 0.49f && blue.a < 0.51f && red.a > 0.49f && green.a > 0.49f && amber.a > 0.49f,
		"player colors are about 50% alpha");
	expect(blue.b > blue.r && red.r > red.b && green.g > green.r && amber.r > amber.b,
		"1P blue, 2P red, 3P green, 4P amber");

	for(int p = 0; p < 4; ++p)
	{
		for(auto kind : {AnchorKind::DPad, AnchorKind::Face, AnchorKind::Start, AnchorKind::L, AnchorKind::R})
		{
			auto a = defaultAnchor(p, kind);
			expect(anchorInOwnQuadrant(p, a), "anchor stays in its quadrant and out of y 42-58");
		}
	}
	expect(defaultAnchor(0, AnchorKind::DPad).x == 10.f && defaultAnchor(0, AnchorKind::DPad).y == 80.f, "1P d-pad");
	expect(defaultAnchor(1, AnchorKind::Face).x == 90.f && defaultAnchor(3, AnchorKind::Start).y == 6.f, "2P face and 4P start");

	expect(syncModeButton(4, true, false) == ModeSync::Keep, "removed mode stays gone at 4 players");
	expect(syncModeButton(2, true, false) == ModeSync::Keep, "removed mode stays gone at 2 players");
	expect(syncModeButton(1, false, false) == ModeSync::Create, "1 player still shows the mode button");
	expect(syncModeButton(1, true, false) == ModeSync::Keep, "a removed mode button stays hidden at 1 player");
	expect(syncModeButton(1, true, true) == ModeSync::Erase, "a stale mode button is dropped when removal was saved");
	expect(removalSurvivesPlayerCountChange(true), "changing 2 -> 1 -> 4 keeps the removal");
	bool removed = true;
	int count = 2;
	count = 1;
	count = 4;
	expect(count == 4 && removalSurvivesPlayerCountChange(removed), "cycle ends at 4 with the flag set");
	expect(syncModeButton(count, removed, false) == ModeSync::Keep, "mode button does not reappear after reload");

	std::vector<SavedButton> sets{
		{0, 1, 100, -200, 0, false, false},
		{1, 2, 300, -400, 9, false, true},
		{2, 1, 10, 20, 0, true, false},
		{3, 1, 500, -600, 1, false, false},
	};
	auto blob = encodeButtons(sets, true, 4);
	auto loaded = decodeButtons(blob);
	expect(loaded.playerCount == 4, "player count 4 is restored");
	expect(loaded.modeRemoved, "bit 7 / mode flag is restored");
	expect(loaded.buttons.size() == 4, "four sets reload");
	expect(loaded.buttons[0].x == 100 && loaded.buttons[0].y == -200, "P1 position reloads");
	expect(loaded.buttons[1].hidden, "P2 hidden state reloads");
	expect(loaded.buttons[1].deviceId == 1, "P2 cannot fire as another player");
	expect(loaded.buttons[3].deviceId == 3, "P4 stays on port 3 even if the saved id was wrong");
	expect((loaded.emptied & 0b0100) != 0, "P3 emptied marker reloads");
	expect(playersNeedingDefaults(4, loaded.present, loaded.emptied) == 0, "emptied P3 is not refilled");

	uint8_t oldPresent = 0b0011;
	uint8_t oldEmpty = 0;
	expect(playersNeedingDefaults(4, oldPresent, oldEmpty) == 0b1100, "old 2-player save gains P3 and P4");
	expect(playersNeedingDefaults(2, oldPresent, oldEmpty) == 0, "a 2-player system does not invent P3");
	expect(restoredPlayerCount(false, 0, true, 2) == 2, "NEO-Duo old config stays 2 players");
	expect(restoredPlayerCount(false, 0, true, 12) == 2, "an old 2-player save stays 2 on a 4-player system");
	expect(restoredPlayerCount(false, 0, false, 12) == 1, "an old 1-player save stays 1");
	expect(restoredPlayerCount(true, 4, true, 12) == 4, "an explicit count of 4 is restored");
	expect(onScreenSetActive(0, 2) && onScreenSetActive(1, 2), "P1 and P2 stay active at 2 players");
	expect(!onScreenSetActive(2, 2) && !onScreenSetActive(3, 2), "P3 and P4 are inactive at 2 players");
	expect(onScreenSetActive(-1, 2), "a shared control stays active");
	expect(syncModeButton(2, false, false) == ModeSync::Create, "resetting UI groups brings a removed Mode button back");

	auto onlyP3 = encodeButtons({{2, 1, 1, 1, 2, true, false}}, false, 4);
	auto p3 = decodeButtons(onlyP3);
	expect((p3.emptied & 0b0100) != 0 && (p3.emptied & 0b1011) == 0, "deleting P3 does not empty anyone else");
	expect(!p3.modeRemoved, "deleting P3 does not remove the mode button");

	expect(fanOutPlayers(4) == 4 && fanOutPlayers(2) == 2 && fanOutPlayers(1) == 1, "shared keys fan out to every active player");
	expect(port1MultitapRequired(4) && !port1MultitapRequired(2), "multitap is only required for 4 players");

	auto ext = encodeQuadExtension({4, 1200, 1300, 64, 80});
	auto back = decodeQuadExtension(ext);
	expect(ext.size() == kQuadExtensionBytes && back.playerCount == 4 && back.size4 == 1300 && back.alpha3 == 64,
		"quad extension round-trips with no extra bytes");

	if(fails)
	{
		std::cerr << fails << " failure(s)\n";
		return EXIT_FAILURE;
	}
	std::cout << "quadControlsTest ok\n";
	return EXIT_SUCCESS;
}
