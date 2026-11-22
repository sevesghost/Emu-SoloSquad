#include <emuframework/VController.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/AppKeyCode.hh>
#include <emuframework/AppMeta.hh>
#include <emuframework/QuadControls.hh>
#include <emuframework/QuadLayout.hh>
#include <algorithm>
#include <format>

namespace EmuEx
{

// Saturn Quad: four players, one joint pad plus a pad per player.
// NEO-Duo (two players) keeps its own builder in NEO.emu.
static bool isQuad() { return AppMeta::maxPlayers > 2; }

static void stampDuoElement(VControllerElement &elem, int16_t sizeMM100x, uint8_t playerSet, uint8_t role, uint8_t alpha)
{
	elem.sizeMM100x = sizeMM100x;
	elem.ownAlpha = alpha;
	elem.duoRole = role;
	elem.duoFlags = VControllerElement::DUO_POS_MM | VControllerElement::DUO_FIXED_PLAYER
		| uint8_t((playerSet & 7) << VControllerElement::DUO_PLAYER_SHIFT);
	if(alpha)
		elem.duoFlags |= VControllerElement::DUO_OWN_ALPHA;
	elem.layoutPos[0] = {LB2DO, {-1, -1}};
	elem.layoutPos[1] = {LB2DO, {-1, -1}};
}

uint8_t VController::alphaForSet(uint8_t setField) const
{
	int idx = playerIndexFromSet(setField);
	if(idx >= 0 && idx < maxDuoPlayers)
		return playerAlpha_[idx];
	return sharedAlpha_;
}

int VController::playerFaceSizeMM100x(int player) const
{
	if(player < 0 || player >= maxDuoPlayers)
		return playerSize_[0];
	return playerSize_[player];
}

int VController::playerOpacity(int player) const
{
	if(player < 0 || player >= maxDuoPlayers)
		return playerAlpha_[0];
	return playerAlpha_[player];
}

int VController::activePlayerCount() const
{
	int n = clampPlayerCount(playerControls_);
	int cap = AppMeta::maxPlayers < 1 ? 1 : AppMeta::maxPlayers;
	if(cap > maxDuoPlayers)
		cap = maxDuoPlayers;
	return n < cap ? n : cap;
}

// NEO-Duo swaps to the classic single-player buttons with one player.
// Saturn Quad keeps its duo elements and uses the joint pad as the 1P pad.
bool VController::usesDuoSets() const
{
	return twoPlayer_ || isQuad();
}

VControllerElement &VController::addDuoButton(KeyInfo key, int16_t sizeMM100x, uint8_t playerSet, uint8_t role)
{
	VControllerButtonGroup::Config conf;
	conf.keys = {key};
	conf.layout.rowItems = 1;
	conf.layout.origin = LB2DO;
	auto &elem = gpElements.emplace_back(std::in_place_type<VControllerButtonGroup>, conf);
	stampDuoElement(elem, sizeMM100x, playerSet, role, alphaForSet(playerSet));
	if(hasWindow())
		update(elem);
	return elem;
}

VControllerElement &VController::addDuoDPad(std::span<const KeyInfo, 4> keys, int16_t sizeMM100x, uint8_t playerSet, uint8_t role)
{
	auto &elem = gpElements.emplace_back(std::in_place_type<VControllerDPad>, keys);
	stampDuoElement(elem, sizeMM100x, playerSet, role, alphaForSet(playerSet));
	if(hasWindow())
	{
		update(elem);
		if(elem.dPad())
			elem.dPad()->updateBoundingAreaGfx(renderer());
	}
	return elem;
}

void VController::setDuoPosition(VControllerElement &elem, int layoutIdx, _2DOrigin origin, float xMm, float yMm)
{
	if(elem.isEmptiedSetMarker())
		return;
	elem.duoFlags |= VControllerElement::DUO_POS_MM;
	elem.layoutPos[layoutIdx].origin = origin;
	elem.layoutPos[layoutIdx].pos = {int16_t(std::lround(xMm * 10.f)), int16_t(std::lround(yMm * 10.f))};
}

// A player's own pad, as opposed to the joint pad or its P1..P4 strip.
static bool isSeparateSetElement(const VControllerElement &elem)
{
	return quadPartOf(elem.duoRole) == QuadPart::Separate;
}

bool VController::playerSetWasEmptied(int player) const
{
	if(player < 0 || player >= maxDuoPlayers)
		return false;
	return std::ranges::any_of(gpElements, [&](const VControllerElement &elem)
	{
		return elem.isEmptiedSetMarker() && isSeparateSetElement(elem) && elem.playerIndex() == player;
	});
}

bool VController::hasRealPlayerElements(int player) const
{
	if(player < 0 || player >= maxDuoPlayers)
		return false;
	return std::ranges::any_of(gpElements, [&](const VControllerElement &elem)
	{
		return !elem.isEmptiedSetMarker() && isSeparateSetElement(elem) && elem.playerIndex() == player;
	});
}

uint8_t VController::emptiedPlayerMask() const
{
	uint8_t mask{};
	for(int p = 0; p < maxDuoPlayers; ++p)
	{
		if(playerSetWasEmptied(p))
			mask = uint8_t(mask | uint8_t(1u << p));
	}
	return mask;
}

uint8_t VController::presentPlayerMask() const
{
	uint8_t mask{};
	for(int p = 0; p < maxDuoPlayers; ++p)
	{
		if(hasRealPlayerElements(p))
			mask = uint8_t(mask | uint8_t(1u << p));
	}
	return mask;
}

void VController::ensureEmptyPlayerMarker(int player)
{
	if(player < 0 || player >= maxDuoPlayers || playerSetWasEmptied(player))
		return;
	VControllerButtonGroup::Config conf;
	conf.layout.rowItems = 1;
	conf.layout.origin = LB2DO;
	auto &elem = gpElements.emplace_back(std::in_place_type<VControllerButtonGroup>, conf);
	auto set = setFieldForPlayer(player);
	stampDuoElement(elem, 0, set, uint8_t(player), 0);
	elem.state = VControllerState::OFF;
	elem.layoutPos[0] = {LB2DO, {kEmptySetPos, kEmptySetPos}};
	elem.layoutPos[1] = {LB2DO, {kEmptySetPos, kEmptySetPos}};
}

void VController::applyEmptiedMask(uint8_t mask)
{
	if(!mask)
		return;
	std::erase_if(gpElements, [&](const VControllerElement &elem)
	{
		int p = elem.playerIndex();
		if(p < 0 || elem.isEmptiedSetMarker() || !isSeparateSetElement(elem))
			return false;
		return (mask & uint8_t(1u << p)) != 0;
	});
	for(int p = 0; p < maxDuoPlayers; ++p)
	{
		if(mask & uint8_t(1u << p))
			ensureEmptyPlayerMarker(p);
	}
}

bool VController::hasJointElements() const
{
	return std::ranges::any_of(gpElements, [](const VControllerElement &elem)
	{
		return !elem.isEmptiedSetMarker() && isJointRole(elem.duoRole) && elem.duoRole != kJointRoleAdded;
	});
}

bool VController::jointSetWasEmptied() const
{
	return std::ranges::any_of(gpElements, [](const VControllerElement &elem)
	{
		return elem.isEmptiedSetMarker() && elem.duoRole == kJointRoleEmptied;
	});
}

void VController::ensureJointEmptiedMarker()
{
	if(jointSetWasEmptied())
		return;
	VControllerButtonGroup::Config conf;
	conf.layout.rowItems = 1;
	conf.layout.origin = LB2DO;
	auto &elem = gpElements.emplace_back(std::in_place_type<VControllerButtonGroup>, conf);
	stampDuoElement(elem, 0, 2, kJointRoleEmptied, 0);
	elem.state = VControllerState::OFF;
	elem.layoutPos[0] = {LB2DO, {kEmptySetPos, kEmptySetPos}};
	elem.layoutPos[1] = {LB2DO, {kEmptySetPos, kEmptySetPos}};
}

bool VController::elementIsActive(const VControllerElement &elem) const
{
	if(elem.isEmptiedSetMarker())
		return false;
	if(!isQuad())
		return onScreenSetActive(elem.playerIndex(), activePlayerCount());
	auto part = quadPartOf(elem.duoRole);
	int player = part == QuadPart::JointPlayer ? jointRolePlayer(elem.duoRole) : elem.playerIndex();
	return quadPartVisible(part, player, activePlayerCount(), duoLayout_ == DuoLayout::Joint);
}

uint16_t VController::jointTargetPorts() const
{
	return EmuEx::jointTargetPorts(linkMask_, activePlayerCount(), inputPlayer_);
}

static void clearModeRemoved(std::vector<VControllerElement> &uiElements);

void VController::setTwoPlayerControls(bool on)
{
	// Saturn's 1/2/4 menu calls setPlayerControls, so a count change keeps
	// dragged sets and a removed Mode button. NEO-Duo's boolean toggle swaps
	// the classic layout and the duo layout. Turning it back on brings the
	// Mode button back. Joint/Separate does not call this.
	if(AppMeta::maxPlayers > 2)
	{
		setPlayerControls(on ? 2 : 1);
		return;
	}
	if(twoPlayer_ == on)
		return;
	twoPlayer_ = on;
	playerControls_ = int8_t(on ? 2 : 1);
	duoCustomized_ = false;
	resetEmulatedDeviceGroups();
	if(on)
	{
		modeButtonRemoved_ = false;
		clearModeRemoved(uiElements);
	}
	syncDuoModeButton();
	if(hasWindow())
	{
		place();
		app().viewController().placeEmuViews();
	}
}

void VController::setPlayerControls(int players)
{
	players = clampPlayerCount(players);
	if(playerControls_ == players)
		return;
	// Let go of anything held: the joint pad's targets and the visible pads change.
	if(hasWindow())
		resetInput();
	int previous = playerControls_;
	// The Mode-button removal flag is global and must survive this change.
	// Do not rebuild the groups: positions and HIDDEN state stay put.
	playerControls_ = int8_t(players);
	twoPlayer_ = players > 1;
	if(players > previous || isQuad())
		supplementMissingPlayers();
	AppMeta::setPort1Multitap(port1MultitapRequired(players));
	syncDuoModeButton();
	if(hasWindow())
	{
		place();
		applySavedButtonAlpha();
		app().viewController().placeEmuViews();
	}
}

void VController::setDuoLayout(DuoLayout layout)
{
	if(duoLayout_ == layout)
		return;
	if(hasWindow())
		resetInput();
	duoLayout_ = layout;
	if(AppMeta::maxPlayers <= 2)
	{
		// NEO-Duo rebuilds the default groups. This does not clear a removed
		// Mode button, so Joint/Separate keeps that choice.
		duoCustomized_ = false;
		if(twoPlayer_)
			resetEmulatedDeviceGroups();
	}
	// Quad keeps both the joint pad and the per-player pads, with their own
	// positions. The layout only decides which ones are live.
	if(hasWindow())
	{
		place();
		applySavedButtonAlpha();
		app().viewController().placeEmuViews();
	}
}

void VController::setShowSharedFaceRow(bool on)
{
	if(showSharedRow_ == on)
		return;
	showSharedRow_ = on;
	duoCustomized_ = false;
	if(twoPlayer_)
		resetEmulatedDeviceGroups(true);
	if(hasWindow())
	{
		place();
		app().viewController().placeEmuViews();
	}
}

static void applyOpacity(VControllerElement &elem, uint8_t alpha)
{
	elem.ownAlpha = alpha;
	if(alpha)
		elem.duoFlags |= VControllerElement::DUO_OWN_ALPHA;
}

void VController::setPlayerFaceSizeMM100x(int player, int mm100x)
{
	mm100x = std::clamp(mm100x, 900, 3000);
	if(player < 0)
		player = 0;
	if(player >= maxDuoPlayers)
		player = maxDuoPlayers - 1;
	if(lockPlayerSizes_)
	{
		for(auto &size : playerSize_)
			size = int16_t(mm100x);
	}
	else
		playerSize_[player] = int16_t(mm100x);
	if(duoCustomized_)
		AppMeta::refreshDuoButtonSizes(*this);
	if(hasWindow())
		place();
}

void VController::setSharedFaceSizeMM100x(int mm100x)
{
	sharedSize_ = int16_t(std::clamp(mm100x, 900, 3000));
	if(duoCustomized_)
		AppMeta::refreshDuoButtonSizes(*this);
	if(hasWindow())
		place();
}

void VController::setPlayerOpacity(int player, uint8_t alpha)
{
	if(player < 0 || player >= maxDuoPlayers)
		return;
	playerAlpha_[player] = alpha;
	auto field = setFieldForPlayer(player);
	for(auto &elem : gpElements)
	{
		if(elem.isEmptiedSetMarker() || elem.playerSet() != field)
			continue;
		applyOpacity(elem, alpha);
	}
	applySavedButtonAlpha();
}

void VController::setSharedOpacity(uint8_t alpha)
{
	sharedAlpha_ = alpha;
	for(auto &elem : gpElements)
	{
		if(!elem.isSharedSet())
			continue;
		applyOpacity(elem, alpha);
	}
	applySavedButtonAlpha();
}

void VController::setLockPlayerSizes(bool on)
{
	lockPlayerSizes_ = on;
	if(on)
		setPlayerFaceSizeMM100x(0, playerSize_[0]);
}

void VController::setVideoBottomInsetMM(int layout, float mm)
{
	if(layout >= 0 && layout < 2)
		bottomInsetMM_[layout] = mm;
}

int VController::activeGamepadLayoutIndex() const
{
	if(!hasWindow())
		return 0;
	return int(window().isPortrait());
}

int VController::videoBottomInsetPx() const
{
	if(!twoPlayer_ || !hasWindow())
		return 0;
	int idx = activeGamepadLayoutIndex();
	return std::max(0, yMMSizeToPixel(window(), bottomInsetMM_[idx]));
}

void VController::noteDuoDrag()
{
	duoCustomized_ = true;
}

static bool isDuoModeKey(KeyInfo key)
{
	return key.flags.appCode && AppKeyCode(key.codes[0]) == AppKeyCode::toggleDuoLayout;
}

bool VController::uiElementIsDuoModeButton(const VControllerElement &elem) const
{
	auto *grp = elem.uiButtonGroup();
	if(!grp)
		return false;
	return std::ranges::any_of(grp->buttons, [](const VControllerButton &btn)
	{
		return isDuoModeKey(btn.key);
	});
}

static void stampModeRemoved(std::vector<VControllerElement> &uiElements)
{
	for(auto &elem : uiElements)
		elem.duoFlags = uint8_t(elem.duoFlags | VControllerElement::DUO_MODE_REMOVED);
}

static void clearModeRemoved(std::vector<VControllerElement> &uiElements)
{
	for(auto &elem : uiElements)
		elem.duoFlags = uint8_t(elem.duoFlags & uint8_t(~VControllerElement::DUO_MODE_REMOVED));
}

void VController::noteUIButtonRemoved(KeyInfo key)
{
	if(!isDuoModeKey(key))
		return;
	modeButtonRemoved_ = true;
	stampModeRemoved(uiElements);
}

void VController::noteUIButtonAdded(KeyInfo key)
{
	if(isDuoModeKey(key))
	{
		modeButtonRemoved_ = false;
		clearModeRemoved(uiElements);
		return;
	}
	if(modeButtonRemoved_)
		stampModeRemoved(uiElements);
}

void VController::onLayoutEditorClosed()
{
	app().saveConfigFile(appContext());
}

void VController::ensureDuoModeButton(bool resetToDefault)
{
	if(resetToDefault)
	{
		std::erase_if(uiElements, [&](const VControllerElement &elem)
		{
			return uiElementIsDuoModeButton(elem);
		});
		modeButtonRemoved_ = false;
		clearModeRemoved(uiElements);
	}
	syncDuoModeButton();
}

void VController::syncDuoModeButton()
{
	auto isMode = [](const VControllerElement &elem)
	{
		auto *grp = elem.uiButtonGroup();
		if(!grp)
			return false;
		return std::ranges::any_of(grp->buttons, [](const VControllerButton &btn)
		{
			return btn.key.flags.appCode && AppKeyCode(btn.key.codes[0]) == AppKeyCode::toggleDuoLayout;
		});
	};
	bool present = std::ranges::any_of(uiElements, isMode);
	if(AppMeta::maxPlayers <= 2)
	{
		// NEO-Duo shows the Mode button only while two-player controls are on.
		// An existing button keeps its millimetre position and HIDDEN state.
		// A saved removal stays gone until two-player is turned back on.
		if(!twoPlayer_)
		{
			std::erase_if(uiElements, isMode);
			return;
		}
		if(present || modeButtonRemoved_)
			return;
	}
	else
	{
		switch(syncModeButton(playerControls_, modeButtonRemoved_, present))
		{
			case ModeSync::Erase:
				std::erase_if(uiElements, isMode);
				return;
			case ModeSync::Keep:
				return;
			case ModeSync::Create:
				break;
		}
	}
	VControllerUIButtonGroup::Config conf;
	conf.keys = {KeyInfo::appKey(AppKeyCode::toggleDuoLayout)};
	conf.layout.rowItems = 1;
	conf.layout.origin = CT2DO;
	auto &elem = uiElements.emplace_back(std::in_place_type<VControllerUIButtonGroup>, conf);
	elem.sizeMM100x = 900;
	elem.duoFlags = VControllerElement::DUO_POS_MM;
	if(isQuad())
	{
		// Top-right, under Open Menu: the centre of the screen is game picture
		// and the default pads keep that corner free.
		setDuoPosition(elem, 0, RT2DO, -6.5f, 16.f);
		setDuoPosition(elem, 1, RT2DO, -6.5f, 16.f);
	}
	else
	{
		// Select/Start sit at about 9.5 mm from the top. 30 mm puts this button
		// about 20 mm below them so the 9 mm discs do not overlap.
		setDuoPosition(elem, 0, CT2DO, 0.f, 30.f);
		setDuoPosition(elem, 1, CT2DO, 0.f, 30.f);
	}
	if(hasWindow())
		update(elem);
}

void VController::rebuildDuoIfNeeded()
{
	if(twoPlayer_)
		resetEmulatedDeviceGroups(true);
}

void VController::bindElementToItsPlayer(VControllerElement &elem) const
{
	// Classic elements follow the Virtual Gamepad Player instead.
	if(elem.isEmptiedSetMarker() || !elem.fixedPlayer())
		return;
	auto forEachKey = [&](auto &&fix)
	{
		if(auto *grp = elem.buttonGroup())
		{
			for(auto &btn : grp->buttons)
				fix(btn.key);
		}
		if(auto *dpad = elem.dPad())
		{
			for(auto &key : dpad->config.keys)
				fix(key);
		}
	};
	if(elem.isSharedSet())
	{
		// Shared and joint controls press for every linked player.
		forEachKey([](KeyInfo &key)
		{
			if(key.flags.appCode)
				return;
			key.flags.bothPlayers = 1;
			key.flags.deviceId = 0;
		});
		return;
	}
	int port = elem.playerIndex();
	if(port < 0)
		port = duoRolePort(elem.duoRole);
	if(port < 0 || port > 3)
		return;
	forEachKey([&](KeyInfo &key)
	{
		if(key.flags.appCode)
			return;
		key.flags.bothPlayers = 0;
		key.flags.deviceId = uint8_t(port);
	});
}

static KeyInfo keyForPort(KeyInfo key, int player)
{
	if(!key.flags.appCode)
	{
		key.flags.bothPlayers = 0;
		key.flags.deviceId = uint8_t(player & 3);
	}
	return key;
}

static KeyInfo jointKey(KeyInfo key)
{
	if(!key.flags.appCode)
	{
		key.flags.bothPlayers = 1;
		key.flags.deviceId = 0;
	}
	return key;
}

// What a system's input components are on a pad: the D-pad, the main face
// button group, the left and right shoulder triggers, and single buttons
// such as Start.
enum class PadPiece : uint8_t { None, DPad, Face, L, R, Start, PlayerStart, Link };

static PadPiece pieceOfComponent(const InputComponentDesc &c)
{
	if(c.flags.altConfig || c.keyCodes.empty())
		return PadPiece::None;
	if(c.type == InputComponent::dPad)
		return c.keyCodes.size() >= 4 ? PadPiece::DPad : PadPiece::None;
	if(c.keyCodes.size() > 1)
		return c.type == InputComponent::button ? PadPiece::Face : PadPiece::None;
	if(c.type == InputComponent::trigger)
		return c.layoutOrigin.xScaler() == 1 ? PadPiece::R : PadPiece::L;
	return PadPiece::Start;
}

// Saved separate pads only carry the player in their role, so a single
// button is recognised by the component its key belongs to.
static PadPiece pieceOfElement(const VControllerElement &elem, int &singlesSeen)
{
	switch(elem.duoRole)
	{
		case kJointRoleDpad: return PadPiece::DPad;
		case kJointRoleFace: return PadPiece::Face;
		case kJointRoleL: return PadPiece::L;
		case kJointRoleR: return PadPiece::R;
		case kJointRoleStart: return PadPiece::Start;
		default: break;
	}
	if(isJointPlayerRole(elem.duoRole))
		return elem.duoRole >= kJointRoleLink ? PadPiece::Link : PadPiece::PlayerStart;
	if(elem.dPad())
		return PadPiece::DPad;
	auto *grp = elem.buttonGroup();
	if(!grp || grp->buttons.empty())
		return PadPiece::None;
	if(grp->buttons.size() > 1)
		return PadPiece::Face;
	auto codes = grp->buttons[0].key.codes;
	for(const auto &c : AppMeta::inputDeviceDesc(0).components)
	{
		if(c.flags.altConfig || c.keyCodes.size() != 1 || c.keyCodes[0].codes != codes)
			continue;
		if(auto piece = pieceOfComponent(c); piece != PadPiece::None)
			return piece;
	}
	// A reassigned button: fall back to the build order L, R, Start.
	int n = singlesSeen++;
	return n == 0 ? PadPiece::L : n == 1 ? PadPiece::R : PadPiece::Start;
}

void VController::buildGenericQuadSets()
{
	int players = activePlayerCount();
	auto desc = AppMeta::inputDeviceDesc(0);
	for(int p = 0; p < players; ++p)
	{
		if(playerSetWasEmptied(p))
		{
			ensureEmptyPlayerMarker(p);
			continue;
		}
		if(hasRealPlayerElements(p))
			continue;
		auto set = setFieldForPlayer(p);
		auto role = uint8_t(p);
		bool placedDpad{};
		bool placedFace{};
		for(const auto &component : desc.components)
		{
			auto piece = pieceOfComponent(component);
			if(piece == PadPiece::DPad && !placedDpad)
			{
				std::array<KeyInfo, 4> keys{
					keyForPort(component.keyCodes[0], p),
					keyForPort(component.keyCodes[1], p),
					keyForPort(component.keyCodes[2], p),
					keyForPort(component.keyCodes[3], p)};
				addDuoDPad(keys, 2000, set, role);
				placedDpad = true;
			}
			else if(piece == PadPiece::Face && !placedFace)
			{
				VControllerButtonGroup::Config conf;
				for(auto key : component.keyCodes)
					conf.keys.push_back(keyForPort(key, p));
				conf.layout.rowItems = component.keyCodes.size() > 3 ? int8_t(3) : int8_t(component.keyCodes.size());
				conf.layout.origin = LB2DO;
				auto &elem = gpElements.emplace_back(std::in_place_type<VControllerButtonGroup>, conf);
				stampDuoElement(elem, 1000, set, role, alphaForSet(set));
				if(hasWindow())
					update(elem);
				placedFace = true;
			}
			else if(piece == PadPiece::L || piece == PadPiece::R || piece == PadPiece::Start)
			{
				addDuoButton(keyForPort(component.keyCodes[0], p), 1000, set, role);
			}
		}
	}
	buildJointSet();
}

// The joint pad: one D-pad, face group, L, R and Start that press for every
// linked player, then each player's Start and link chip.
void VController::buildJointSet()
{
	if(!isQuad() || hasJointElements() || jointSetWasEmptied())
		return;
	constexpr uint8_t shared = 2;
	auto desc = AppMeta::inputDeviceDesc(0);
	KeyInfo startKey{};
	bool placedDpad{}, placedFace{}, placedL{}, placedR{}, placedStart{};
	for(const auto &component : desc.components)
	{
		switch(pieceOfComponent(component))
		{
			case PadPiece::DPad:
			{
				if(placedDpad)
					break;
				std::array<KeyInfo, 4> keys{
					jointKey(component.keyCodes[0]), jointKey(component.keyCodes[1]),
					jointKey(component.keyCodes[2]), jointKey(component.keyCodes[3])};
				addDuoDPad(keys, 2000, shared, kJointRoleDpad);
				placedDpad = true;
				break;
			}
			case PadPiece::Face:
			{
				if(placedFace)
					break;
				VControllerButtonGroup::Config conf;
				for(auto key : component.keyCodes)
					conf.keys.push_back(jointKey(key));
				conf.layout.rowItems = component.keyCodes.size() > 3 ? int8_t(3) : int8_t(component.keyCodes.size());
				conf.layout.origin = LB2DO;
				auto &elem = gpElements.emplace_back(std::in_place_type<VControllerButtonGroup>, conf);
				stampDuoElement(elem, 1000, shared, kJointRoleFace, alphaForSet(shared));
				if(hasWindow())
					update(elem);
				placedFace = true;
				break;
			}
			case PadPiece::L:
				if(!placedL)
					addDuoButton(jointKey(component.keyCodes[0]), 1000, shared, kJointRoleL);
				placedL = true;
				break;
			case PadPiece::R:
				if(!placedR)
					addDuoButton(jointKey(component.keyCodes[0]), 1000, shared, kJointRoleR);
				placedR = true;
				break;
			case PadPiece::Start:
				if(!placedStart)
				{
					addDuoButton(jointKey(component.keyCodes[0]), 1000, shared, kJointRoleStart);
					startKey = component.keyCodes[0];
				}
				placedStart = true;
				break;
			default:
				break;
		}
	}
	for(int p = 0; p < maxDuoPlayers; ++p)
	{
		auto set = setFieldForPlayer(p);
		if(startKey)
			addDuoButton(keyForPort(startKey, p), 1000, set, uint8_t(kJointRolePlayerStart + p));
		auto chip = KeyInfo::appKey(AppKeyCode::togglePlayerLink);
		chip.flags.deviceId = uint8_t(p);
		addDuoButton(chip, 1000, set, uint8_t(kJointRoleLink + p));
	}
}

static float canvasMM(int pixels, int pixelsPer100MM)
{
	return std::max(1.f, pixels / (std::max(1, pixelsPer100MM) / 100.f));
}

// Pick the nearest edge or centre line as the anchor, so a different
// window size keeps each piece next to the edge it was laid out against.
static void placeAt(VController &v, VControllerElement &elem, int layout, float W, float H, QuadLayout::Place p)
{
	Origin ox = Origin::center, oy = Origin::max;
	float x = p.x - W * .5f, y = p.y;
	if(p.x < W / 3.f)
	{
		ox = Origin::min;
		x = p.x;
	}
	else if(p.x > W * 2.f / 3.f)
	{
		ox = Origin::max;
		x = p.x - W;
	}
	if(p.y > H * .5f)
	{
		oy = Origin::min;
		y = p.y - H;
	}
	v.setDuoPosition(elem, layout, _2DOrigin{ox, oy}, x, y);
}

static int16_t mm100(float mm)
{
	return int16_t(std::lround(mm * 100.f));
}

static void setPieceSize(VControllerElement &elem, PadPiece piece, float face, float pill)
{
	switch(piece)
	{
		case PadPiece::DPad: elem.sizeMM100x = mm100(QuadLayout::dpadSize(face)); break;
		case PadPiece::PlayerStart:
		case PadPiece::Link: elem.sizeMM100x = mm100(pill); break;
		default: elem.sizeMM100x = mm100(face); break;
	}
}

void VController::layoutGenericQuadSets(uint8_t onlyPlayers)
{
	if(!hasWindow())
		return;
	float w = canvasMM(window().width(), window().widthMMInPixels(100.f));
	float h = canvasMM(window().height(), window().heightMMInPixels(100.f));
	bool portraitNow = h > w;
	float landW = portraitNow ? h : w;
	float landH = portraitNow ? w : h;
	const bool jointToo = !onlyPlayers || (onlyPlayers & layoutJointPad);
	auto layoutPlayer = [&](int player)
	{
		return player >= 0 && player < maxDuoPlayers && (!onlyPlayers || (onlyPlayers & (1u << player)));
	};
	const int sizeLayout = activeGamepadLayoutIndex();
	const float separateFace = playerFaceSizeMM100x(0) / 100.f;
	const float jointFace = sharedFaceSizeMM100x() / 100.f;
	const int count = activePlayerCount();
	// Index 0 is landscape, index 1 portrait (the same edges swapped), so a
	// rotation needs no new layout.
	for(int layout = 0; layout < 2; ++layout)
	{
		const float W = layout == 0 ? landW : landH;
		const float H = layout == 0 ? landH : landW;
		const auto joint = QuadLayout::joint(W, H, jointFace);
		// P1/P2 use the two-pad layout until four players are on screen.
		// P3/P4 always use the four-pad spots.
		const auto pair = QuadLayout::separate(W, H, separateFace, count >= 4 ? 4 : 2);
		const auto quad = QuadLayout::separate(W, H, separateFace, 4);
		int singles[maxDuoPlayers + 1]{};
		for(auto &elem : gpElements)
		{
			if(elem.isEmptiedSetMarker())
				continue;
			auto part = quadPartOf(elem.duoRole);
			if(part == QuadPart::Always)
				continue;
			bool isJoint = part == QuadPart::Joint || part == QuadPart::JointPlayer;
			int player = isJoint ? -1 : elem.playerIndex();
			if(isJoint ? !jointToo : !layoutPlayer(player))
				continue;
			auto piece = pieceOfElement(elem, singles[player + 1]);
			QuadLayout::Place place{};
			float face{};
			if(isJoint)
			{
				face = joint.face;
				switch(piece)
				{
					case PadPiece::DPad: place = joint.pad.dpad; break;
					case PadPiece::Face: place = joint.pad.face; break;
					case PadPiece::L: place = joint.pad.l; break;
					case PadPiece::R: place = joint.pad.r; break;
					case PadPiece::Start: place = joint.pad.start; break;
					case PadPiece::PlayerStart: place = joint.playerStart[jointRolePlayer(elem.duoRole)]; break;
					case PadPiece::Link: place = joint.link[jointRolePlayer(elem.duoRole)]; break;
					default: break;
				}
			}
			else
			{
				const auto &sep = player < 2 ? pair : quad;
				const auto &set = sep.sets[player];
				face = sep.face;
				switch(piece)
				{
					case PadPiece::DPad: place = set.dpad; break;
					case PadPiece::Face: place = set.face; break;
					case PadPiece::L: place = set.l; break;
					case PadPiece::R: place = set.r; break;
					case PadPiece::Start: place = set.start; break;
					default: break;
				}
			}
			if(!place.used)
				continue;
			placeAt(*this, elem, layout, W, H, place);
			// One size per element: the current orientation's fit. A rotation
			// runs this again.
			if(layout == sizeLayout)
				setPieceSize(elem, piece, face, joint.pill);
		}
	}
}

void VController::refreshGenericQuadSizes()
{
	const float jointFace = std::max(9.f, sharedFaceSizeMM100x() / 100.f);
	const float pill = std::clamp(jointFace, 9.f, 12.f);
	int singles[maxDuoPlayers + 1]{};
	for(auto &elem : gpElements)
	{
		if(elem.isEmptiedSetMarker() || quadPartOf(elem.duoRole) == QuadPart::Always)
			continue;
		bool isJoint = isJointRole(elem.duoRole);
		int p = isJoint ? -1 : elem.playerIndex();
		if(!isJoint && p < 0)
			continue;
		float face = isJoint ? jointFace : std::max(9.f, playerFaceSizeMM100x(p) / 100.f);
		setPieceSize(elem, pieceOfElement(elem, singles[p + 1]), face, pill);
	}
}

void VController::supplementMissingPlayers()
{
	uint8_t add{};
	if(playerControls_ > 1)
		add = playersNeedingDefaults(activePlayerCount(), presentPlayerMask(), emptiedPlayerMask());
	bool addJoint = isQuad() && !hasJointElements() && !jointSetWasEmptied();
	if(!add && !addJoint)
		return;
	buildGenericQuadSets();
	if(hasWindow())
		layoutGenericQuadSets(uint8_t(add | (addJoint ? layoutJointPad : 0)));
}

// ---- Player links -----------------------------------------------------------

void VController::retargetHeldJointKeys(uint16_t before, uint16_t after)
{
	auto removed = uint16_t(before & ~after);
	auto added = uint16_t(after & ~before);
	if(!removed && !added)
		return;
	auto &app = this->app();
	for(auto &state : dragTracker.stateList())
	{
		for(auto key : state.data)
		{
			if(!key || !key.flags.bothPlayers)
				continue;
			auto direct = key;
			direct.flags.bothPlayers = 0;
			direct.flags.turbo = 0;
			direct.flags.toggle = 0;
			for(int port = 0; port < 16; ++port)
			{
				auto bit = uint16_t(1u << port);
				direct.flags.deviceId = uint8_t(port);
				// A player dropped from the pad lets go of what it was holding.
				if(removed & bit)
					app.handleSystemKeyInput(direct, Input::Action::RELEASED, 0, {.allowTurboModifier = false, .skipHighlight = true});
				// A player joining picks up a held direction or button at once.
				else if((added & bit) && !key.flags.turbo && !key.flags.toggle)
					app.handleSystemKeyInput(direct, Input::Action::PUSHED, 0, {.allowTurboModifier = false, .skipHighlight = true});
			}
		}
	}
}

void VController::setLinkedPlayers(uint8_t mask)
{
	mask = uint8_t(mask & kAllPlayersLinked);
	if(mask == linkMask_)
		return;
	auto before = jointTargetPorts();
	linkMask_ = mask;
	auto after = jointTargetPorts();
	if(hasWindow())
	{
		retargetHeldJointKeys(before, after);
		applySavedButtonAlpha();
	}
}

void VController::onLinkChipReleased(int player, bool held)
{
	const int count = activePlayerCount();
	auto next = held ? linkAfterHold(linkMask_, player, count) : linkAfterTap(linkMask_, player, count);
	if(effectiveLinkMask(next, count) == effectiveLinkMask(linkMask_, count))
	{
		if(!held)
			app().postMessage(1, false, "One player stays on the joint pad");
		return;
	}
	setLinkedPlayers(next);
	auto mask = effectiveLinkMask(linkMask_, count);
	std::string names;
	for(int p = 0; p < count; ++p)
	{
		if(!(mask & (1u << p)))
			continue;
		if(!names.empty())
			names += ' ';
		names += std::format("P{}", p + 1);
	}
	app().postMessage(1, false, std::format("Joint pad: {}", names));
}

[[gnu::weak]] bool AppMeta::buildDuoGamepad(VController &)
{
	return false;
}

[[gnu::weak]] void AppMeta::layoutDuoGamepad(VController &) {}

[[gnu::weak]] void AppMeta::refreshDuoButtonSizes(VController &) {}

[[gnu::weak]] void AppMeta::setPort1Multitap(bool) {}

}
