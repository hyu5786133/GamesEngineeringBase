#pragma once

#include "GamesEngineeringBase.h"
#include "TileRenderer.h"

// ---------------------------------------------------------------------------
// Small HUD helpers. The hearts come from MediavelFull.png: an 18x15 full heart
// at (903, 8) and its faded (alpha ~79) "empty" twin at (967, 8). Because the
// empty heart is semi transparent it needs the alpha blending in TileRenderer.
// ---------------------------------------------------------------------------

// Blends a dark rectangle into the back buffer so the HUD stays readable on any
// terrain (grass, water, trees).
inline void DrawHudBackdrop(GamesEngineeringBase::Window& canvas,
	int x, int y, int w, int h, int alpha)
{
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());

	for (int yy = 0; yy < h; ++yy)
	{
		const int py = y + yy;

		if (py < 0 || py >= winH)
		{
			continue;
		}

		for (int xx = 0; xx < w; ++xx)
		{
			const int px = x + xx;

			if (px < 0 || px >= winW)
			{
				continue;
			}

			unsigned char* destination = canvas.backBuffer()
				+ (static_cast<size_t>(py) * winW + px) * 3;

			const int inverse = 255 - alpha;
			destination[0] = static_cast<unsigned char>(destination[0] * inverse / 255);
			destination[1] = static_cast<unsigned char>(destination[1] * inverse / 255);
			destination[2] = static_cast<unsigned char>(destination[2] * inverse / 255);
		}
	}
}

inline void DrawHearts(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& ui,
	int x, int y, int hp, int maxHp, int scale)
{
	const int heartWidth = 18;
	const int heartHeight = 15;
	const int fullX = 903;
	const int fullY = 8;
	const int emptyX = 967;
	const int emptyY = 8;
	const int step = (heartWidth + 2) * scale;

	DrawHudBackdrop(canvas, x - 4, y - 3, maxHp * step + 4, heartHeight * scale + 6, 110);

	for (int i = 0; i < maxHp; ++i)
	{
		const bool full = i < hp;

		Renderer_blit_scaled(canvas, ui,
			full ? fullX : emptyX,
			full ? fullY : emptyY,
			heartWidth, heartHeight,
			x + i * step, y, scale, false);
	}
}
