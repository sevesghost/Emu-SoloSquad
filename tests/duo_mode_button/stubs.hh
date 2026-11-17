// Minimal host-side stand-ins for the Imagine/EmuFramework types that the
// extracted REAL VController code touches. Only data layout + no-op graphics.
// Everything here is SIMULATED; the logic under test comes from gen/real_code.inc.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace EmuEx
{

struct { template<class... A> void info(A&&...) const {} template<class... A> void error(A&&...) const {} } inline log;

template<class T> auto iotaCount(T n) { return std::views::iota(T{}, n); }
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

constexpr unsigned CFGKEY_VCONTROLLER_UI_BUTTONS_V2 = 113;

struct _2DOrigin
{
	using PackedType = uint8_t;
	uint8_t v{};
	constexpr PackedType pack() const { return v; }
	static constexpr _2DOrigin unpack(PackedType p) { return {p}; }
	constexpr bool operator==(const _2DOrigin &) const = default;
};
constexpr _2DOrigin LT2DO{1}, CT2DO{2}, RT2DO{3}, LC2DO{4}, C2DO{5}, RC2DO{6}, LB2DO{7}, CB2DO{8}, RB2DO{9};

enum class AppKeyCode : uint16_t { openContent = 1, openSystemActions, fastForward, toggleFastForward, rewind, toggleDuoLayout };
enum class AssetID { gamepadOverlay, more, menu };

struct KeyInfo
{
	std::array<uint16_t, 3> codes{};
	struct Flags { uint8_t appCode{}; uint8_t turbo{}; uint8_t toggle{}; constexpr bool operator==(const Flags&) const = default; } flags{};
	static constexpr KeyInfo appKey(AppKeyCode c) { KeyInfo k; k.codes[0] = uint16_t(c); k.flags.appCode = 1; return k; }
	constexpr bool operator==(const KeyInfo &) const = default;
};
static_assert(std::is_trivially_copyable_v<KeyInfo>);

enum class VControllerState : uint8_t { OFF, SHOWN, HIDDEN };

struct VControllerButton { KeyInfo key; };

struct UILayoutCfg { int8_t rowItems{1}; _2DOrigin origin{}; };
struct BtnLayoutCfg { int8_t rowItems{1}; int16_t spacingMM{}; int16_t xPadding{}; int16_t yPadding{}; int8_t staggerType{}; _2DOrigin origin{}; bool showBoundingArea{}; };

template<class Layout>
struct GroupBase
{
	struct Config { std::vector<KeyInfo> keys; Layout layout{}; void validate(auto &) {} };
	std::vector<VControllerButton> buttons;
	Layout layout{};
	GroupBase() = default;
	GroupBase(const Config &c): layout{c.layout} { for(auto k : c.keys) buttons.push_back({k}); }
	Config config() const { Config c; c.layout = layout; for(auto &b : buttons) c.keys.push_back(b.key); return c; }
	size_t configSize() const { return sizeof(Layout) + 1 + buttons.size() * sizeof(KeyInfo); }
};
struct VControllerUIButtonGroup : GroupBase<UILayoutCfg> { using GroupBase::GroupBase; };
struct VControllerButtonGroup : GroupBase<BtnLayoutCfg> { using GroupBase::GroupBase; };
struct VControllerDPad
{
	struct Config { std::array<KeyInfo, 4> keys{}; float diagonalSensitivity{}; int16_t deadzoneMM100x{}; bool visualizeBounds{}; void validate(auto &) {} } config;
	VControllerDPad() = default;
	VControllerDPad(Config c): config{c} {}
	VControllerDPad(std::span<const KeyInfo, 4> k) { std::ranges::copy(k, config.keys.begin()); }
	void updateBoundingAreaGfx(auto &) {}
	size_t configSize() const { return sizeof(Config); }
};

struct VControllerLayoutPosition
{
	_2DOrigin origin{};
	struct Pos { int16_t x{}, y{}; constexpr bool operator==(const Pos&) const = default; } pos{};
};

using ElemVariant = std::variant<VControllerButtonGroup, VControllerUIButtonGroup, VControllerDPad>;
struct VControllerElement : ElemVariant
{
	static constexpr uint8_t DUO_POS_MM = 1 << 0, DUO_FIXED_PLAYER = 1 << 1, DUO_OWN_ALPHA = 1 << 2, DUO_PLAYER_SHIFT = 4;
	std::array<VControllerLayoutPosition, 2> layoutPos{};
	VControllerState state{VControllerState::SHOWN};
	int16_t sizeMM100x{};
	uint8_t ownAlpha{}, duoFlags{}, duoRole{};
	template<class T, class... A> VControllerElement(std::in_place_type_t<T> t, A&&... a): ElemVariant{t, std::forward<A>(a)...} {}
	auto visit(auto &&f) const { return std::visit(f, static_cast<const ElemVariant&>(*this)); }
	auto visit(auto &&f) { return std::visit(f, static_cast<ElemVariant&>(*this)); }
	VControllerUIButtonGroup *uiButtonGroup() { return std::get_if<VControllerUIButtonGroup>(this); }
	const VControllerUIButtonGroup *uiButtonGroup() const { return std::get_if<VControllerUIButtonGroup>(this); }
	VControllerDPad *dPad() { return std::get_if<VControllerDPad>(this); }
	const VControllerDPad *dPad() const { return std::get_if<VControllerDPad>(this); }
	size_t configSize() const { return visit([](auto &e){ return e.configSize(); }) + 2 * (sizeof(VControllerLayoutPosition::Pos) + 1) + 1; }
};

// Byte-buffer stand-in for Imagine's FileIO (writer) and MapIO (reader).
struct ConfigBlob
{
	std::vector<uint8_t> bytes;
	size_t rd{};
	template<class T> void put(const T &v) { static_assert(std::is_trivially_copyable_v<T>); auto p = (const uint8_t*)&v; bytes.insert(bytes.end(), p, p + sizeof(T)); }
	template<class T> T get() { T v; read(v); return v; }
	template<class T> void read(T &v)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		if(rd + sizeof(T) > bytes.size()) throw std::runtime_error("config read past end");
		std::memcpy(&v, bytes.data() + rd, sizeof(T)); rd += sizeof(T);
	}
	size_t size() const { return bytes.size() - rd; }
};
using FileIO = ConfigBlob;
using MapIO = ConfigBlob;

template<class Size, class T> void writeSizedData(FileIO &io, const std::vector<T> &v) { io.put(Size(v.size())); for(auto &e : v) io.put(e); }
template<class Size, class T> void readSizedData(MapIO &io, std::vector<T> &v) { auto n = io.get<Size>(); v.resize(n); for(auto &e : v) io.read(e); }

struct InputManager {};
struct EmuApp
{
	InputManager inputManager;
	struct ViewController { void placeEmuViews() {} } viewController_;
	int asset(AssetID) const { return 0; }
	ViewController &viewController() { return viewController_; }
};
struct Window {};
namespace Gfx
{
struct Renderer { int mainTask{}; };
struct GlyphTextureSet {};
inline std::array<uint8_t, 12> mapFanQuadIndices(int) { return {}; }
}
struct FanQuadIdxs
{
	struct Cfg { int size{}; };
	FanQuadIdxs() = default;
	FanQuadIdxs(int, Cfg) {}
	std::array<uint8_t, 24> map() { return {}; }
};

}
namespace EmuEx { enum class DuoLayout : uint8_t { Separate = 0, Joint = 1 }; }
