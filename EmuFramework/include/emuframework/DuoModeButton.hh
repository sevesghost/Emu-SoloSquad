#pragma once

/*  This file is part of NEO-Duo.

	NEO-Duo is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. */

#include <cstdint>
#include <emuframework/QuadControls.hh>

namespace EmuEx
{

// What configure() should do with the Toggle Control Layout button
// after a config load. Keep leaves a saved button (and its state and
// position) alone. This shim keeps the original host test compiling.
// The live controller uses syncModeButton, which still shows the button
// in 1-player mode unless the user removed it.
enum class DuoModeButtonSync : uint8_t
{
	Keep,
	Create,
	Erase,
};

constexpr bool duoModeButtonShouldExist(bool twoPlayerControls, bool userRemoved) noexcept
{
	return twoPlayerControls && !userRemoved;
}

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
