#pragma once

/*  This file is part of NEO-Duo.

	NEO-Duo is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. */

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace EmuEx
{

inline constexpr int maxDuoPlayers = 4;
inline constexpr int16_t kEmptySetPos = -2;
inline constexpr uint8_t kDuoModeRemovedFlag = 1 << 7;
inline constexpr uint8_t kDuoPlayerShift = 3;
inline constexpr uint8_t kDuoPlayerMask = 7 << kDuoPlayerShift; // bits 3-5, never bit 7

inline constexpr uint8_t kDuoFlagMulti = 1 << 0;
inline constexpr uint8_t kDuoFlagSharedRow = 1 << 1;
inline constexpr uint8_t kDuoFlagCustom = 1 << 2;
inline constexpr uint8_t kDuoFlagSharedBlock = 1 << 3;
inline constexpr uint8_t kDuoFlagModeRemoved = 1 << 4;
inline constexpr uint8_t kDuoFlagPlayerCount = 1 << 5;

// playerCount, p3 size, p4 size, p3 alpha, p4 alpha. Not part of the 5-byte trailer.
inline constexpr size_t kQuadExtensionBytes = 1 + 2 + 2 + 1 + 1;

struct Rgba
{
	float r, g, b, a;
};

enum class AnchorKind : uint8_t { DPad, Face, Start, L, R };

struct AnchorPct
{
	float x, y;
};

// Role 0..3 is the on-screen port. Legacy NEO-Duo button ids still map:
// P2's block starts at 32. Shared buttons stay on port 0 and fan out.
constexpr int duoRolePort(int role) noexcept
{
	if(role >= 0 && role <= 3)
		return role;
	if(role >= 32 && role < 64)
		return 1;
	return 0;
}

constexpr Rgba duoRoleColorRgba(int role) noexcept
{
	constexpr float a = 0.5f;
	switch(duoRolePort(role))
	{
		case 1: return {0xEF / 255.f, 0x44 / 255.f, 0x44 / 255.f, a};
		case 2: return {0x22 / 255.f, 0xC5 / 255.f, 0x5E / 255.f, a};
		case 3: return {0xF5 / 255.f, 0x9E / 255.f, 0x0B / 255.f, a};
		default: return {0x3B / 255.f, 0x82 / 255.f, 0xF6 / 255.f, a};
	}
}

// Legacy field 2 is the shared row. 3 and 4 are 3P and 4P so old value 2 still means shared.
constexpr int playerIndexFromSet(int set) noexcept
{
	switch(set)
	{
		case 0: return 0;
		case 1: return 1;
		case 3: return 2;
		case 4: return 3;
		default: return -1;
	}
}

constexpr uint8_t setFieldForPlayer(int player) noexcept
{
	switch(player)
	{
		case 0: return 0;
		case 1: return 1;
		case 2: return 3;
		case 3: return 4;
		default: return 0;
	}
}

constexpr int clampPlayerCount(int players) noexcept
{
	if(players >= 4)
		return 4;
	if(players == 2)
		return 2;
	return 1;
}

// Old files have no count byte. A 2-player save stays at 2 and does not turn
// on multitap. Four players is only the default for a fresh config.
constexpr int restoredPlayerCount(bool hasCountByte, int savedCount, bool multi, int) noexcept
{
	if(hasCountByte)
		return clampPlayerCount(savedCount);
	return multi ? 2 : 1;
}

// Shared controls (playerIndex < 0) stay active. Sets at or past the current
// player count stay in the config but are not drawn or hit-tested.
constexpr bool onScreenSetActive(int playerIndex, int activeCount) noexcept
{
	return playerIndex < activeCount;
}

// onlyPlayers 0 lays out every real set. A non-zero mask lays out just those
// players, so adding P3/P4 does not move a dragged P1 or P2.
constexpr bool shouldLayoutPlayer(int playerIndex, uint8_t onlyPlayers) noexcept
{
	if(playerIndex < 0 || playerIndex >= maxDuoPlayers)
		return false;
	if(!onlyPlayers)
		return true;
	return (onlyPlayers & uint8_t(1u << playerIndex)) != 0;
}

// 1-player mode still saves every set and empty-set marker.
constexpr bool persistDuoElements(int) noexcept
{
	return true;
}

constexpr uint8_t duoHeaderFlags(bool multi, bool sharedRow, bool custom, bool modeRemoved) noexcept
{
	return uint8_t((multi ? kDuoFlagMulti : 0)
		| (sharedRow ? kDuoFlagSharedRow : 0)
		| (custom ? kDuoFlagCustom : 0)
		| kDuoFlagSharedBlock
		| (modeRemoved ? kDuoFlagModeRemoved : 0)
		| kDuoFlagPlayerCount);
}

struct QuadExtension
{
	uint8_t playerCount{2};
	int16_t size3{1000};
	int16_t size4{1000};
	uint8_t alpha3{};
	uint8_t alpha4{};
};

inline void storeI16LE(uint8_t *dst, int16_t v) noexcept
{
	auto u = static_cast<uint16_t>(v);
	dst[0] = uint8_t(u & 0xff);
	dst[1] = uint8_t((u >> 8) & 0xff);
}

inline int16_t loadI16LE(const uint8_t *src) noexcept
{
	return int16_t(uint16_t(src[0]) | (uint16_t(src[1]) << 8));
}

inline std::array<uint8_t, kQuadExtensionBytes> encodeQuadExtension(QuadExtension ext) noexcept
{
	std::array<uint8_t, kQuadExtensionBytes> out{};
	out[0] = uint8_t(clampPlayerCount(ext.playerCount));
	storeI16LE(&out[1], ext.size3);
	storeI16LE(&out[3], ext.size4);
	out[5] = ext.alpha3;
	out[6] = ext.alpha4;
	return out;
}

inline QuadExtension decodeQuadExtension(std::span<const uint8_t, kQuadExtensionBytes> in) noexcept
{
	QuadExtension ext;
	ext.playerCount = uint8_t(clampPlayerCount(in[0]));
	ext.size3 = loadI16LE(&in[1]);
	ext.size4 = loadI16LE(&in[3]);
	ext.alpha3 = in[5];
	ext.alpha4 = in[6];
	return ext;
}

constexpr bool isEmptiedSetPos(int16_t x0, int16_t y0, int16_t x1, int16_t y1) noexcept
{
	return x0 == kEmptySetPos && y0 == kEmptySetPos && x1 == kEmptySetPos && y1 == kEmptySetPos;
}

// Bits of players who still need a default set. An emptied player has a marker
// and is not refilled. A player missing from an old 2-player save is refilled.
constexpr uint8_t playersNeedingDefaults(int playerCount, uint8_t presentMask, uint8_t emptyMask) noexcept
{
	uint8_t add{};
	int n = playerCount < 0 ? 0 : playerCount;
	if(n > maxDuoPlayers)
		n = maxDuoPlayers;
	for(int p = 0; p < n; ++p)
	{
		auto bit = uint8_t(1u << p);
		if((presentMask & bit) || (emptyMask & bit))
			continue;
		add = uint8_t(add | bit);
	}
	return add;
}

enum class ModeSync : uint8_t { Keep, Create, Erase };

// Show the Mode button at 1, 2, and 4 players. A saved removal (bit 7)
// keeps it hidden and is not cleared by the player count.
constexpr ModeSync syncModeButton(int, bool userRemoved, bool present) noexcept
{
	if(userRemoved)
		return present ? ModeSync::Erase : ModeSync::Keep;
	return present ? ModeSync::Keep : ModeSync::Create;
}

constexpr bool removalSurvivesPlayerCountChange(bool removed) noexcept
{
	return removed;
}

constexpr AnchorPct defaultAnchor(int player, AnchorKind kind) noexcept
{
	// Landscape percentages, origin top-left, y down. The middle band stays clear.
	// L/R sit just inside the set, toward the center, still outside y 42-58.
	constexpr AnchorPct sets[4][5] = {
		{{10.f, 80.f}, {38.f, 80.f}, {24.f, 94.f}, {16.f, 68.f}, {32.f, 68.f}},
		{{62.f, 80.f}, {90.f, 80.f}, {76.f, 94.f}, {68.f, 68.f}, {84.f, 68.f}},
		{{10.f, 20.f}, {38.f, 20.f}, {24.f, 6.f}, {16.f, 32.f}, {32.f, 32.f}},
		{{62.f, 20.f}, {90.f, 20.f}, {76.f, 6.f}, {68.f, 32.f}, {84.f, 32.f}},
	};
	int p = player < 0 ? 0 : (player > 3 ? 3 : player);
	return sets[p][int(kind)];
}

constexpr bool anchorInOwnQuadrant(int player, AnchorPct a) noexcept
{
	bool left = player == 0 || player == 2;
	bool bottom = player == 0 || player == 1;
	bool xOk = left ? a.x < 50.f : a.x > 50.f;
	bool yOk = bottom ? a.y > 58.f : a.y < 42.f;
	return xOk && yOk;
}

constexpr const char *playerFaceLabel(int playerIndex, bool shared) noexcept
{
	if(shared)
		return "Both ";
	switch(playerIndex)
	{
		case 1: return "2P ";
		case 2: return "3P ";
		case 3: return "4P ";
		default: return "1P ";
	}
}

// On-screen 4-player mode uses slots 1-4 of a Port 1 multitap.
// In 1-player mode the Virtual Gamepad Player (0-based inputPlayer) picks
// the Saturn port: 0-1 are the two native ports, 2-5 (P3-P6) need a Port 1
// multitap, and 6-11 (P7-P12) need both taps so those indices land on
// Port 2's six slots (see SMPC MapPorts).
constexpr bool port1MultitapRequired(int playerCount, int inputPlayer = 0) noexcept
{
	if(playerCount >= 4)
		return true;
	if(playerCount <= 1)
		return inputPlayer >= 2;
	return false;
}

constexpr bool port2MultitapRequired(int playerCount, int inputPlayer = 0) noexcept
{
	return playerCount <= 1 && inputPlayer >= 6;
}

// The Mode button switches Joint/Separate only after a press that began on
// the button and was held for the required time. A release after a press
// that started elsewhere must not toggle the layout.
constexpr bool modeHoldTogglesLayout(bool pressBeganOnMode, bool heldLongEnough) noexcept
{
	return pressBeganOnMode && heldLongEnough;
}

// A Mode press stays pending only until that same pointer is released or
// canceled, including a release that ends off the button.
constexpr bool modePressRemainsPending(bool pending, bool samePointer, bool releasedOrCanceled) noexcept
{
	if(pending && samePointer && releasedOrCanceled)
		return false;
	return pending;
}

// Mid-game, never drop an overlay tap that is already on (dropping remaps
// P2). Adding a new tap is deferred only for a 2P/4P layout, because
// enabling a Port 1 tap remaps P2 from physical Port 2 onto Port 1 slot 2.
// In 1P there is no second on-screen player, so a newly needed tap (P3+)
// is applied immediately. A load or reset (gameLoaded=false) applies
// wanted as-is.
constexpr bool midGameOverlayTap(bool currentlyOn, bool wanted, bool gameLoaded, bool singlePlayer = false) noexcept
{
	if(!gameLoaded)
		return wanted;
	if(currentlyOn)
		return true;
	if(singlePlayer)
		return wanted;
	return false;
}

constexpr bool midGameOverlayDeferred(bool currentlyOn, bool wanted, bool gameLoaded, bool singlePlayer = false) noexcept
{
	return gameLoaded && wanted && !currentlyOn && !singlePlayer;
}

// Exclusive end of the gamepad slots forced while an overlay tap is on.
// Port 1 always covers slots 0-3 (4P). 1P extends through the selected
// player when that player is on Port 1 (P5-P6 are slots 4-5). Port 2
// covers only the selected 1P slot (P7-P12), not every unused slot.
constexpr int overlayForcedPadEnd(int port, int playerCount, int inputPlayer) noexcept
{
	if(port == 0)
	{
		int end = 4;
		if(playerCount <= 1 && inputPlayer >= 0 && inputPlayer < 6 && inputPlayer + 1 > end)
			end = inputPlayer + 1;
		return end;
	}
	if(port == 1)
	{
		if(playerCount <= 1 && inputPlayer >= 6 && inputPlayer < 12)
			return inputPlayer + 1;
		return 6;
	}
	return 0;
}

// Fan a shared button out to every active port, not only 0 and 1.
constexpr int fanOutPlayers(int playerCount) noexcept
{
	int n = clampPlayerCount(playerCount);
	return n < 1 ? 1 : n;
}

// ---- Saturn Quad joint pad ------------------------------------------------
// Joint layout: one pad whose presses go to every linked player, plus a strip
// with each player's Start and a link chip. Separate layout: one pad per player
// (roles 0..3, as saved by v1.2). Both sets stay in the config, so switching
// keeps each set's positions. NEO-Duo uses roles 0..69 and never reaches 128.
inline constexpr uint8_t kJointRoleDpad = 128;
inline constexpr uint8_t kJointRoleFace = 129;
inline constexpr uint8_t kJointRoleL = 130;
inline constexpr uint8_t kJointRoleR = 131;
inline constexpr uint8_t kJointRoleStart = 132;
// A group added from "Add New Button Group". Shown in every layout and
// pressed for every linked player.
inline constexpr uint8_t kJointRoleAdded = 133;
// Saved when the user deletes every joint element, so it is not rebuilt.
inline constexpr uint8_t kJointRoleEmptied = 134;
inline constexpr uint8_t kJointRolePlayerStart = 136; // 136..139 = P1..P4 Start
inline constexpr uint8_t kJointRoleLink = 140;        // 140..143 = P1..P4 link chip
inline constexpr uint8_t kJointRoleEnd = 144;

constexpr bool isJointRole(uint8_t role) noexcept
{
	return role >= kJointRoleDpad && role < kJointRoleEnd;
}

constexpr bool isJointPlayerRole(uint8_t role) noexcept
{
	return role >= kJointRolePlayerStart && role < kJointRoleEnd;
}

// P1..P4 index of a per-player joint element, or -1.
constexpr int jointRolePlayer(uint8_t role) noexcept
{
	return isJointPlayerRole(role) ? int(role - kJointRolePlayerStart) & 3 : -1;
}

enum class QuadPart : uint8_t
{
	Separate,    // one player's own pad
	Joint,       // the shared pad
	JointPlayer, // a player's Start or link chip in the joint strip
	Always,      // a group the user added
};

constexpr QuadPart quadPartOf(uint8_t role) noexcept
{
	if(role == kJointRoleAdded)
		return QuadPart::Always;
	if(isJointPlayerRole(role))
		return QuadPart::JointPlayer;
	if(isJointRole(role))
		return QuadPart::Joint;
	return QuadPart::Separate;
}

// One player always uses the joint pad: it is a normal single pad then.
// With 2 or 4 players the layout picks the joint pad or the separate pads.
constexpr bool quadPartVisible(QuadPart part, int player, int playerCount, bool jointLayout) noexcept
{
	const bool single = playerCount <= 1;
	switch(part)
	{
		case QuadPart::Always: return true;
		case QuadPart::Joint: return single || jointLayout;
		case QuadPart::JointPlayer: return !single && jointLayout && player >= 0 && player < playerCount;
		case QuadPart::Separate: return !single && !jointLayout && player >= 0 && player < playerCount;
	}
	return false;
}

// ---- Player links -----------------------------------------------------------
// Bit p set: the joint pad drives player p. Not saved; every launch starts
// with all players linked.
inline constexpr uint8_t kAllPlayersLinked = 0x0F;

constexpr uint8_t activePlayerBits(int playerCount) noexcept
{
	int n = playerCount < 1 ? 1 : (playerCount > maxDuoPlayers ? maxDuoPlayers : playerCount);
	return uint8_t((1u << n) - 1u);
}

// Linked players among the active ones. Never empty: with nobody linked the
// pad drives every active player.
constexpr uint8_t effectiveLinkMask(uint8_t linked, int playerCount) noexcept
{
	auto active = activePlayerBits(playerCount);
	auto mask = uint8_t(linked & active);
	return mask ? mask : active;
}

// Tap on a chip: link or unlink that player. The last linked player stays.
constexpr uint8_t linkAfterTap(uint8_t linked, int player, int playerCount) noexcept
{
	auto active = activePlayerBits(playerCount);
	if(player < 0 || player >= maxDuoPlayers || !(active & (1u << player)))
		return linked;
	auto next = uint8_t(effectiveLinkMask(linked, playerCount) ^ (1u << player));
	if(!next)
		return linked;
	return uint8_t((linked & ~active) | next);
}

// Hold on a chip: drive only that player. Holding the only linked player's
// chip links everyone again.
constexpr uint8_t linkAfterHold(uint8_t linked, int player, int playerCount) noexcept
{
	auto active = activePlayerBits(playerCount);
	if(player < 0 || player >= maxDuoPlayers || !(active & (1u << player)))
		return linked;
	auto bit = uint8_t(1u << player);
	if(effectiveLinkMask(linked, playerCount) == bit)
		return kAllPlayersLinked;
	return bit;
}

// Ports a joint press reaches. With one on-screen player that is the
// "Virtual Gamepad Player" port, which may be past P4.
constexpr uint16_t jointTargetPorts(uint8_t linked, int playerCount, int inputPlayer) noexcept
{
	if(playerCount <= 1)
		return uint16_t(1u << (inputPlayer < 0 ? 0 : (inputPlayer & 15)));
	return effectiveLinkMask(linked, playerCount);
}

}
