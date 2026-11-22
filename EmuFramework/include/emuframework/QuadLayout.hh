#pragma once

/*  This file is part of Saturn Quad (Emu-SoloSquad).

	Saturn Quad is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. */

// Default positions for Saturn Quad's on-screen controls, in millimetres.
// Pure geometry with no framework dependencies so the host tests can check
// that nothing overlaps on real phone and tablet sizes.
//
// Coordinates: x grows to the right, y grows down, (0, 0) is the top-left of
// the canvas. Every Place is the centre of the piece.

#include <algorithm>
#include <array>

namespace EmuEx::QuadLayout
{

// VControllerButtonGroup's default spacing between buttons of one group.
inline constexpr float groupSpacing = 2.f;
// A button accepts touches this far past its drawn edge (the group spacing).
inline constexpr float touchPad = 2.f;
// Distance kept from the screen edge.
inline constexpr float margin = 4.f;
// The default UI buttons sit in the top corners: fast-forward and rewind on
// the left, Open Menu and the Mode button on the right. Game buttons stay out.
inline constexpr float cornerW = 13.f;
inline constexpr float cornerH = 21.f;
// Smallest button the automatic layouts will shrink to on a crowded screen.
inline constexpr float minFace = 7.5f;
inline constexpr float maxFace = 20.f;

constexpr float dpadSize(float face) { return face * 2.f; }
constexpr float faceGroupW(float face) { return face * 3.f + groupSpacing * 2.f; }
constexpr float faceGroupH(float face) { return face * 2.f + groupSpacing; }
// Start-style pills are drawn half as tall as they are wide.
constexpr float pillH(float width) { return width * .5f; }
constexpr float clampFace(float face) { return std::clamp(face, minFace, maxFace); }

struct Place
{
	float x{}, y{};
	float size{}; // D-pad edge, or one button's edge (pill width for pills)
	bool used{};
};

struct Pieces
{
	Place dpad, face, l, r, start;
};

struct Joint
{
	Pieces pad;
	std::array<Place, 4> playerStart{};
	std::array<Place, 4> link{};
	float face{};
	float pill{};
	bool stripAtBottom{};
};

struct Separate
{
	std::array<Pieces, 4> sets{};
	float face{};
	bool fits{};
	bool cornersKept{}; // false only on screens too small for four pads
};

enum class Kind : unsigned char { DPad, Face, Square, Pill };

struct Box
{
	float l{}, t{}, r{}, b{};

	constexpr bool overlaps(const Box &o, float eps = .01f) const
	{
		return l < o.r - eps && o.l < r - eps && t < o.b - eps && o.t < b - eps;
	}
	constexpr bool inside(float w, float h, float eps = .01f) const
	{
		return l >= -eps && t >= -eps && r <= w + eps && b <= h + eps;
	}
	constexpr Box grown(float m) const { return {l - m, t - m, r + m, b + m}; }
};

// The drawn rectangle of a piece.
constexpr Box drawnBox(Place p, Kind kind)
{
	float w{}, h{};
	switch(kind)
	{
		case Kind::DPad: w = h = p.size; break;
		case Kind::Face: w = faceGroupW(p.size); h = faceGroupH(p.size); break;
		case Kind::Square: w = h = p.size; break;
		case Kind::Pill: w = p.size; h = pillH(p.size); break;
	}
	return {p.x - w * .5f, p.y - h * .5f, p.x + w * .5f, p.y + h * .5f};
}

// Where a touch is accepted. The D-pad reads a square 1.5x its drawn size;
// buttons take precedence over a D-pad when both are hit.
constexpr Box touchBox(Place p, Kind kind)
{
	auto box = drawnBox(p, kind);
	if(kind == Kind::DPad)
		return box.grown(p.size * .25f);
	return box.grown(touchPad);
}

constexpr bool inTopCorner(Box b, float w)
{
	Box left{0.f, 0.f, cornerW, cornerH};
	Box right{w - cornerW, 0.f, w, cornerH};
	return b.overlaps(left) || b.overlaps(right);
}

// One pad for every linked player, plus the per-player strip:
// P1..P4 Start pills and P1..P4 link chips.
//
// Landscape: D-pad bottom-left, A B C / X Y Z bottom-right, L and R above them,
// Start and the player strip between the two halves at the bottom. On a canvas
// too narrow for that the strip moves to the top centre.
// Portrait: the same pad along the bottom, Start between L and R, and the strip
// above the shoulder buttons.
constexpr Joint joint(float W, float H, float faceWanted)
{
	Joint j{};
	const bool portrait = W < H;
	float f = clampFace(faceWanted);
	// Shrink the pad until the cluster fits under the top corners and the
	// D-pad and face buttons do not meet.
	for(;;)
	{
		float d = dpadSize(f);
		float rowH = std::max(d, faceGroupH(f));
		bool wide = margin + d + 3.f + faceGroupW(f) + margin <= W;
		bool tall = H - margin - rowH - 3.f - f >= cornerH + 1.f;
		if((wide && tall) || f <= minFace)
			break;
		f = std::max(minFace, f - .25f);
	}
	const float d = dpadSize(f), gw = faceGroupW(f), gh = faceGroupH(f);
	const float rowH = std::max(d, gh);
	const float pill = std::clamp(f, 9.f, 12.f);
	const float ph = pillH(pill);
	j.face = f;
	j.pill = pill;
	j.pad.dpad = {margin + d * .5f, H - margin - d * .5f, d, true};
	j.pad.face = {W - margin - gw * .5f, H - margin - gh * .5f, f, true};
	const float lrY = H - margin - rowH - 3.f - f * .5f;
	j.pad.l = {margin + f * .5f, lrY, f, true};
	j.pad.r = {W - margin - f * .5f, lrY, f, true};

	const float pitch = pill + touchPad * 2.f;
	const float stripW = pitch * 3.f + pill;
	const float dpadTouchR = j.pad.dpad.x + d * .75f;
	const float faceTouchL = j.pad.face.x - gw * .5f - touchPad;
	auto columnX = [&](int i) { return W * .5f + (float(i) - 1.5f) * pitch; };
	auto putStrip = [&](float startRowY, float linkRowY)
	{
		for(int i = 0; i < 4; ++i)
		{
			j.playerStart[i] = {columnX(i), startRowY, pill, true};
			j.link[i] = {columnX(i), linkRowY, pill, true};
		}
	};
	// Rows are 2.5mm apart edge to edge, so the 2mm touch pads do not meet.
	const float rowStep = ph + 2.5f + touchPad;
	if(!portrait)
	{
		bool bottomFits = W * .5f - stripW * .5f - touchPad >= dpadTouchR + 1.f
			&& W * .5f + stripW * .5f + touchPad <= faceTouchL - 1.f;
		if(bottomFits)
		{
			// From the bottom edge: Start, link chips, per-player Start.
			float startY = H - margin - ph * .5f;
			j.pad.start = {W * .5f, startY, pill, true};
			putStrip(startY - rowStep * 2.f, startY - rowStep);
			j.stripAtBottom = true;
		}
		else
		{
			// Narrow landscape: strip at the top centre, between the corners.
			float topY = margin + ph * .5f;
			putStrip(topY, topY + rowStep);
			float startX = W * .5f;
			float startY = H - margin - ph * .5f;
			bool startFits = startX - pill * .5f - touchPad >= dpadTouchR + 1.f
				&& startX + pill * .5f + touchPad <= faceTouchL - 1.f;
			if(!startFits)
				startY = lrY;
			j.pad.start = {startX, startY, pill, true};
		}
	}
	else
	{
		j.pad.start = {W * .5f, lrY, pill, true};
		float lrTop = lrY - f * .5f;
		float linkY = lrTop - 3.f - touchPad - ph * .5f;
		putStrip(linkY - rowStep, linkY);
	}
	return j;
}

constexpr float clusterW(float f, float gap) { return dpadSize(f) + gap + f + gap + faceGroupW(f); }
constexpr float clusterH(float f, float gap) { return std::max(dpadSize(f), faceGroupH(f)) + gap + f; }

// One player's pad inside the box whose top-left is (x0, y0):
//   L ........................ R
//   D-pad  Start  X Y Z / A B C
constexpr Pieces cluster(float x0, float y0, float f, float gap)
{
	Pieces p{};
	const float d = dpadSize(f), gw = faceGroupW(f), gh = faceGroupH(f);
	const float cw = clusterW(f, gap);
	const float rowTop = y0 + f + gap;
	const float rowBottom = rowTop + std::max(d, gh);
	p.l = {x0 + f * .5f, y0 + f * .5f, f, true};
	p.r = {x0 + cw - f * .5f, y0 + f * .5f, f, true};
	p.dpad = {x0 + d * .5f, rowBottom - d * .5f, d, true};
	p.start = {x0 + d + gap + f * .5f, rowBottom - pillH(f) * .5f, f, true};
	p.face = {x0 + cw - gw * .5f, rowBottom - gh * .5f, f, true};
	return p;
}

// One pad per player. Two players share the bottom edge (P1 left, P2 right).
// Four players use the quadrants: P1/P2 bottom, P3/P4 top, with the top pads
// kept out of the UI corners. Portrait stacks the pads from the bottom up.
// Buttons shrink (not below minFace) before anything overlaps. Pieces of
// different groups stay touchPad apart so one group's touch margin never
// covers another group's drawn button.
constexpr Separate separate(float W, float H, float faceWanted, int players)
{
	Separate s{};
	const int n = players >= 4 ? 4 : 2;
	const bool portrait = W < H;
	const float edge = 2.f;
	const float gap = touchPad;
	const float padGap = touchPad * 2.f; // between two players' pads
	float f = clampFace(faceWanted);
	bool corners = true;
	auto fits = [&](float f)
	{
		float cw = clusterW(f, gap), ch = clusterH(f, gap);
		if(portrait)
		{
			float stack = edge + ch * float(n) + padGap * float(n - 1);
			// The top pad stays below the corner buttons unless it is narrow
			// enough to sit between them.
			bool topClear = !corners || cw <= W - cornerW * 2.f - 2.f || stack + cornerH + 1.f <= H;
			return cw <= W - edge * 2.f && stack <= H - edge && topClear;
		}
		bool bottomRow = cw * 2.f + edge * 2.f + padGap <= W;
		if(n == 2)
			return bottomRow && ch + edge <= H - (corners ? cornerH + 1.f : edge);
		bool topRow = cw <= W * .5f - (corners ? cornerW + 1.f : edge) - padGap * .5f;
		bool rows = edge + ch + padGap + ch + edge <= H;
		return bottomRow && topRow && rows;
	};
	while(!fits(f) && f > minFace)
		f = std::max(minFace, f - .25f);
	if(!fits(f))
	{
		// A small phone cannot hold four pads next to the corner buttons.
		// Let the pads reach the corners (the UI buttons still win there)
		// rather than overlap each other.
		corners = false;
		f = clampFace(faceWanted);
		while(!fits(f) && f > minFace)
			f = std::max(minFace, f - .25f);
	}
	s.face = f;
	s.fits = fits(f);
	s.cornersKept = corners;
	const float cw = clusterW(f, gap), ch = clusterH(f, gap);
	if(portrait)
	{
		float x0 = (W - cw) * .5f;
		for(int p = 0; p < n; ++p)
		{
			float y0 = H - edge - ch * float(p + 1) - padGap * float(p);
			s.sets[p] = cluster(x0, y0, f, gap);
		}
		return s;
	}
	const float bottomY = H - edge - ch;
	s.sets[0] = cluster(edge, bottomY, f, gap);
	s.sets[1] = cluster(W - edge - cw, bottomY, f, gap);
	if(n == 4)
	{
		const float topY = edge;
		const float side = corners ? cornerW + 1.f : edge;
		s.sets[2] = cluster(side, topY, f, gap);
		s.sets[3] = cluster(W - side - cw, topY, f, gap);
	}
	return s;
}

}
