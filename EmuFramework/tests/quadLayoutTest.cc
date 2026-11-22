// Host test for Saturn Quad's default on-screen layouts.
// g++ -std=c++20 -Wall -Werror -I EmuFramework/include EmuFramework/tests/quadLayoutTest.cc -o /tmp/quadLayoutTest && /tmp/quadLayoutTest
//
// For common phone and tablet sizes, in landscape and portrait, every default
// layout must keep each drawn piece on screen, out of the top UI corners, and
// clear of every other piece's touch area. Only the D-pad's oversized touch
// square may reach other buttons: buttons are hit-tested before D-pads.

#include <emuframework/QuadLayout.hh>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace EmuEx::QuadLayout;

static int failures;

#define CHECK(cond, ...) do { if(!(cond)) { ++failures; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while(0)

struct Piece
{
	std::string name;
	Place place;
	Kind kind;
};

static void addPieces(std::vector<Piece> &out, const std::string &prefix, const Pieces &p)
{
	out.push_back({prefix + "dpad", p.dpad, Kind::DPad});
	out.push_back({prefix + "face", p.face, Kind::Face});
	out.push_back({prefix + "L", p.l, Kind::Square});
	out.push_back({prefix + "R", p.r, Kind::Square});
	out.push_back({prefix + "start", p.start, Kind::Pill});
}

static void checkPieces(const char *what, float w, float h, float face, const std::vector<Piece> &pieces, bool corners = true)
{
	for(auto &a : pieces)
	{
		CHECK(a.place.used, "%s %gx%g f%g: %s unused", what, w, h, face, a.name.c_str());
		auto drawn = drawnBox(a.place, a.kind);
		CHECK(drawn.inside(w, h), "%s %gx%g f%g: %s off screen (%g,%g)-(%g,%g)", what, w, h, face, a.name.c_str(),
			drawn.l, drawn.t, drawn.r, drawn.b);
		if(corners)
			CHECK(!inTopCorner(drawn, w), "%s %gx%g f%g: %s under a top-corner UI button", what, w, h, face, a.name.c_str());
		for(auto &b : pieces)
		{
			if(&a == &b || b.kind == Kind::DPad)
				continue;
			CHECK(!touchBox(b.place, b.kind).overlaps(drawn), "%s %gx%g f%g: %s touch area covers %s", what, w, h, face,
				b.name.c_str(), a.name.c_str());
		}
	}
}

static void checkJoint(float w, float h, float face)
{
	auto j = joint(w, h, face);
	std::vector<Piece> pieces;
	addPieces(pieces, "", j.pad);
	for(int i = 0; i < 4; ++i)
	{
		pieces.push_back({"P" + std::to_string(i + 1) + "start", j.playerStart[i], Kind::Pill});
		pieces.push_back({"P" + std::to_string(i + 1) + "link", j.link[i], Kind::Pill});
	}
	checkPieces("joint", w, h, face, pieces);
	CHECK(j.face <= clampFace(face), "joint %gx%g: face grew", w, h);
	CHECK(j.face >= minFace, "joint %gx%g: face below minimum", w, h);
	// The chips read left to right as P1..P4.
	for(int i = 1; i < 4; ++i)
		CHECK(j.link[i].x > j.link[i - 1].x, "joint %gx%g: chips out of order", w, h);
}

static void checkSeparate(float w, float h, float face, int players)
{
	auto s = separate(w, h, face, players);
	CHECK(s.fits, "separate%d %gx%g f%g: does not fit (face %g)", players, w, h, face, s.face);
	// Phones 145mm long or more keep the corners clear, even with four pads in portrait.
	if(std::max(w, h) >= 145.f)
		CHECK(s.cornersKept, "separate%d %gx%g f%g: pads reach the UI corners", players, w, h, face);
	std::vector<Piece> pieces;
	for(int p = 0; p < players; ++p)
		addPieces(pieces, "P" + std::to_string(p + 1), s.sets[p]);
	checkPieces(players == 4 ? "separate4" : "separate2", w, h, face, pieces, s.cornersKept);
	CHECK(s.face <= clampFace(face), "separate %gx%g: face grew", w, h);
	if(w > h)
	{
		// Landscape: P1 left of P2; with four players P3/P4 are above P1/P2.
		CHECK(s.sets[0].dpad.x < s.sets[1].dpad.x, "separate %gx%g: P1 not left of P2", w, h);
		if(players == 4)
		{
			CHECK(s.sets[2].dpad.y < s.sets[0].dpad.y, "separate %gx%g: P3 not above P1", w, h);
			CHECK(s.sets[3].dpad.y < s.sets[1].dpad.y, "separate %gx%g: P4 not above P2", w, h);
		}
	}
}

int main()
{
	// Landscape canvases in mm: phones (20:9, 19.5:9, 16:9, small), foldable, tablets.
	const float sizes[][2]
	{
		{151, 68}, {160, 74}, {146, 68}, {140, 65}, {130, 73}, {123, 69},
		{175, 82}, {180, 110}, {200, 150}, {220, 140}, {250, 175},
	};
	const float faces[]{9, 10, 12, 14, 16, 20};
	for(const auto &[lw, lh] : sizes)
	{
		for(float face : faces)
		{
			checkJoint(lw, lh, face);
			checkJoint(lh, lw, face);
			for(int players : {2, 4})
			{
				checkSeparate(lw, lh, face, players);
				checkSeparate(lh, lw, face, players);
			}
		}
	}
	// A default 10mm pad is kept at 10mm on a typical phone in Joint mode and
	// in two-player Separate mode.
	CHECK(joint(151, 68, 10).face == 10.f, "joint phone face %g", joint(151, 68, 10).face);
	CHECK(joint(151, 68, 10).stripAtBottom, "joint phone strip not at the bottom");
	CHECK(separate(151, 68, 10, 2).face == 10.f, "separate2 phone face %g", separate(151, 68, 10, 2).face);
	// Four pads on a phone shrink, but not below the upstream default of 8.5mm.
	CHECK(separate(151, 68, 10, 4).face >= 8.5f, "separate4 phone face %g", separate(151, 68, 10, 4).face);
	// A tablet keeps big buttons.
	CHECK(separate(250, 175, 14, 4).face == 14.f, "separate4 tablet face %g", separate(250, 175, 14, 4).face);
	if(failures)
	{
		std::printf("quadLayoutTest: %d failure(s)\n", failures);
		return 1;
	}
	std::printf("quadLayoutTest ok\n");
	return 0;
}
