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

// hp and maxHp are counted in HALF hearts (2 = one heart), so partial damage
// can show the half heart sprite at (935, 8)
inline void DrawHearts(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& ui,
	int x, int y, int hpHalf, int maxHalf, int scale)
{
	const int heartWidth = 18;
	const int heartHeight = 15;
	const int fullX = 903;
	const int halfX = 935;
	const int emptyX = 967;
	const int srcY = 8;
	const int step = (heartWidth + 2) * scale;

	const int hearts = maxHalf / 2;

	DrawHudBackdrop(canvas, x - 4, y - 3, hearts * step + 4, heartHeight * scale + 6, 110);

	for (int i = 0; i < hearts; ++i)
	{
		const int value = hpHalf - i * 2;
		const int srcX = (value >= 2) ? fullX : ((value == 1) ? halfX : emptyX);

		Renderer_blit_scaled(canvas, ui, srcX, srcY, heartWidth, heartHeight,
			x + i * step, y, scale, false);
	}
}
