#include <emuframework/VController.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/AppKeyCode.hh>
#include <emuframework/AppMeta.hh>
#include <emuframework/QuadControls.hh>
#include <algorithm>

namespace EmuEx
{

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

static void placePercent(VController &v, VControllerElement &elem, int layout, float canvasW, float canvasH, AnchorPct a)
{
	float x = canvasW * (a.x / 100.f);
	float y = canvasH * (a.y / 100.f);
	v.setDuoPosition(elem, layout, LB2DO, x, y - canvasH);
}

bool VController::playerSetWasEmptied(int player) const
{
	if(player < 0 || player >= maxDuoPlayers)
		return false;
	for(const auto &elem : gpElements)
	{
		if(elem.isEmptiedSetMarker() && elem.playerIndex() == player)
			return true;
	}
	return false;
}

bool VController::hasRealPlayerElements(int player) const
{
	if(player < 0 || player >= maxDuoPlayers)
		return false;
	for(const auto &elem : gpElements)
	{
		if(elem.isEmptiedSetMarker() || elem.playerIndex() != player)
			continue;
		return true;
	}
	return false;
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
		if(p < 0 || elem.isEmptiedSetMarker())
			return false;
		return (mask & uint8_t(1u << p)) != 0;
	});
	for(int p = 0; p < maxDuoPlayers; ++p)
	{
		if(mask & uint8_t(1u << p))
			ensureEmptyPlayerMarker(p);
	}
}

void VController::setTwoPlayerControls(bool on)
{
	setPlayerControls(on ? 2 : 1);
}

void VController::setPlayerControls(int players)
{
	players = clampPlayerCount(players);
	if(playerControls_ == players)
		return;
	int previous = playerControls_;
	// The Mode-button removal flag is global and must survive this change.
	// Do not rebuild the groups: positions and HIDDEN state stay put.
	playerControls_ = int8_t(players);
	twoPlayer_ = players > 1;
	if(players > previous)
		supplementMissingPlayers();
	AppMeta::setPort1Multitap(port1MultitapRequired(players));
	syncDuoModeButton();
	if(hasWindow())
	{
		place();
		app().viewController().placeEmuViews();
	}
}

void VController::setDuoLayout(DuoLayout layout)
{
	if(duoLayout_ == layout)
		return;
	duoLayout_ = layout;
	// Keep every button and the Mode removal flag. Joint/Separate only retargets
	// anchors when the player has not dragged anything.
	if(hasWindow())
	{
		place();
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
	VControllerUIButtonGroup::Config conf;
	conf.keys = {KeyInfo::appKey(AppKeyCode::toggleDuoLayout)};
	conf.layout.rowItems = 1;
	conf.layout.origin = CT2DO;
	auto &elem = uiElements.emplace_back(std::in_place_type<VControllerUIButtonGroup>, conf);
	elem.sizeMM100x = 900;
	elem.duoFlags = VControllerElement::DUO_POS_MM;
	setDuoPosition(elem, 0, CT2DO, 0.f, 30.f);
	setDuoPosition(elem, 1, CT2DO, 0.f, 30.f);
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
	if(elem.isEmptiedSetMarker() || elem.isSharedSet())
		return;
	int port = elem.playerIndex();
	if(port < 0)
		port = duoRolePort(elem.duoRole);
	if(port < 0 || port > 3)
		return;
	auto fix = [&](KeyInfo &key)
	{
		if(key.flags.appCode || key.flags.bothPlayers)
			return;
		key.flags.deviceId = uint8_t(port);
	};
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
		int singles{};
		for(const auto &component : desc.components)
		{
			if(component.flags.altConfig || component.keyCodes.empty())
				continue;
			if(component.type == InputComponent::dPad && component.keyCodes.size() >= 4 && !placedDpad)
			{
				std::array<KeyInfo, 4> keys{
					keyForPort(component.keyCodes[0], p),
					keyForPort(component.keyCodes[1], p),
					keyForPort(component.keyCodes[2], p),
					keyForPort(component.keyCodes[3], p)};
				addDuoDPad(keys, 2000, set, role);
				placedDpad = true;
				continue;
			}
			if(component.type == InputComponent::button && component.keyCodes.size() > 1 && !placedFace)
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
				continue;
			}
			AnchorKind kind = AnchorKind::Start;
			if(singles == 1)
				kind = AnchorKind::L;
			else if(singles >= 2)
				kind = AnchorKind::R;
			(void)kind;
			addDuoButton(keyForPort(component.keyCodes[0], p), 900, set, role);
			++singles;
		}
	}
}

void VController::layoutGenericQuadSets(uint8_t onlyPlayers)
{
	if(!hasWindow())
		return;
	float w = std::max(1.f, window().width() / float(std::max(1, window().widthMMInPixels(1.f))));
	float h = std::max(1.f, window().height() / float(std::max(1, window().heightMMInPixels(1.f))));
	bool portraitNow = h > w;
	float landW = portraitNow ? h : w;
	float landH = portraitNow ? w : h;
	auto layoutOne = [&](int layout, float canvasW, float canvasH)
	{
		int singles[4]{};
		for(auto &elem : gpElements)
		{
			if(elem.isEmptiedSetMarker())
				continue;
			int p = elem.playerIndex();
			if(!shouldLayoutPlayer(p, onlyPlayers))
				continue;
			AnchorKind kind = AnchorKind::Face;
			if(elem.dPad())
				kind = AnchorKind::DPad;
			else if(elem.buttonGroup() && elem.buttonGroup()->buttons.size() <= 1)
			{
				int n = singles[p]++;
				kind = n == 0 ? AnchorKind::Start : n == 1 ? AnchorKind::L : AnchorKind::R;
			}
			placePercent(*this, elem, layout, canvasW, canvasH, defaultAnchor(p, kind));
		}
	};
	layoutOne(0, landW, landH);
	layoutOne(1, landH, landW);
}

void VController::refreshGenericQuadSizes()
{
	for(auto &elem : gpElements)
	{
		if(elem.isEmptiedSetMarker())
			continue;
		int p = elem.playerIndex();
		if(p < 0)
			continue;
		float face = std::max(9.f, playerFaceSizeMM100x(p) / 100.f);
		float size = elem.dPad() ? face * 2.f : face;
		elem.sizeMM100x = int16_t(std::lround(size * 100.f));
	}
}

void VController::supplementMissingPlayers()
{
	if(playerControls_ <= 1)
		return;
	auto add = playersNeedingDefaults(activePlayerCount(), presentPlayerMask(), emptiedPlayerMask());
	if(!add)
		return;
	buildGenericQuadSets();
	if(hasWindow())
		layoutGenericQuadSets(add);
}

[[gnu::weak]] bool AppMeta::buildDuoGamepad(VController &)
{
	return false;
}

[[gnu::weak]] void AppMeta::layoutDuoGamepad(VController &) {}

[[gnu::weak]] void AppMeta::refreshDuoButtonSizes(VController &) {}

[[gnu::weak]] void AppMeta::setPort1Multitap(bool) {}

}
