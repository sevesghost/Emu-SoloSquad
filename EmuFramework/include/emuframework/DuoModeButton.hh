#pragma once

/*  This file is part of NEO-Duo.

	NEO-Duo is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. */

#include <cstdint>

namespace EmuEx
{

// What configure() should do with the Toggle Control Layout button
// after a config load. Keep leaves a saved button (and its state and
// position) alone.
enum class DuoModeButtonSync : uint8_t
{
	Keep,
	Create,
	Erase,
};

// userRemoved defaults to false. Configs written before that field
// existed therefore still show the button while two-player controls are on.
constexpr bool duoModeButtonShouldExist(bool twoPlayerControls, bool userRemoved) noexcept
{
	return twoPlayerControls && !userRemoved;
}

// Keep means the loaded button is left untouched: its millimetre position
// and its state (including HIDDEN) survive relaunch.
// resetToDefault forces a fresh SHOWN button at the default position
// (Reset UI Groups, or two-player toggled back on after the caller
// clears userRemoved).
constexpr DuoModeButtonSync decideDuoModeButton(bool twoPlayerControls, bool userRemoved, bool presentInLoadedConfig, bool resetToDefault = false) noexcept
{
	if(resetToDefault)
		userRemoved = false;
	bool shouldExist = duoModeButtonShouldExist(twoPlayerControls, userRemoved);
	if(shouldExist && (resetToDefault || !presentInLoadedConfig))
		return DuoModeButtonSync::Create;
	if(!shouldExist && presentInLoadedConfig)
		return DuoModeButtonSync::Erase;
	return DuoModeButtonSync::Keep;
}

}
