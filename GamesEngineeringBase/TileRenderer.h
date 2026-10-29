#pragma once

#include "GamesEngineeringBase.h"

// ---------------------------------------------------------------------------
// Low level blitting helpers used by the tile map.
//
// The framework only offers full-image drawing (Renderer_draw_image), so a tile
// map needs to copy 32x32 regions out of a tileset. These helpers do that and
// honour the source alpha channel, which is what makes transparent tiles and
// sprites work.
// ---------------------------------------------------------------------------

inline void Renderer_blit(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& image,
	int srcX, int srcY, int w, int h,
	int dstX, int dstY)
{
	const unsigned char* src = image.data;
	if (src == nullptr || w <= 0 || h <= 0)
	{
		return;
	}

	const int channels = static_cast<int>(image.channels);
	const int imgW = static_cast<int>(image.width);
	const int imgH = static_cast<int>(image.height);

	// Clip the source rectangle to the source image
	if (srcX < 0) { w += srcX; dstX -= srcX; srcX = 0; }
	if (srcY < 0) { h += srcY; dstY -= srcY; srcY = 0; }
	if (srcX + w > imgW) { w = imgW - srcX; }
	if (srcY + h > imgH) { h = imgH - srcY; }
	if (w <= 0 || h <= 0)
	{
		return;
	}

	// Clip the destination rectangle to the window
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());
	int x0 = 0, y0 = 0;
	if (dstX < 0) { x0 = -dstX; }
	if (dstY < 0) { y0 = -dstY; }
	int x1 = w, y1 = h;
	if (dstX + x1 > winW) { x1 = winW - dstX; }
	if (dstY + y1 > winH) { y1 = winH - dstY; }

	for (int y = y0; y < y1; ++y)
	{
		const unsigned char* row = src + ((srcY + y) * imgW + srcX) * channels;

		for (int x = x0; x < x1; ++x)
		{
			const unsigned char* p = row + x * channels;

			// Fully transparent pixels are skipped so the layer below shows through
			if (channels == 4 && p[3] == 0)
			{
				continue;
			}

			canvas.draw(dstX + x, dstY + y, const_cast<unsigned char*>(p));
		}
	}
}

// Draws one tile of a tileset (specified in tile coordinates, not pixels)
inline void Renderer_draw_tile(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& tileset,
	int col, int row, int tileSize,
	int dstX, int dstY)
{
	Renderer_blit(canvas, tileset,
		col * tileSize, row * tileSize, tileSize, tileSize,
		dstX, dstY);
}

// Same as Renderer_blit but scales each source pixel up by an integer factor and
// can mirror the sprite horizontally (used by the animated characters).
inline void Renderer_blit_scaled(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& image,
	int srcX, int srcY, int srcW, int srcH,
	int dstX, int dstY, int scale, bool flipX)
{
	const unsigned char* src = image.data;

	if (src == nullptr || srcW <= 0 || srcH <= 0 || scale <= 0)
	{
		return;
	}

	const int channels = static_cast<int>(image.channels);
	const int imgW = static_cast<int>(image.width);
	const int imgH = static_cast<int>(image.height);
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());

	for (int y = 0; y < srcH; ++y)
	{
		const int sy = srcY + y;

		if (sy < 0 || sy >= imgH)
		{
			continue;
		}

		const int py0 = dstY + y * scale;

		if (py0 + scale <= 0 || py0 >= winH)
		{
			continue;
		}

		for (int x = 0; x < srcW; ++x)
		{
			const int sx = srcX + (flipX ? (srcW - 1 - x) : x);

			if (sx < 0 || sx >= imgW)
			{
				continue;
			}

			const unsigned char* p = src + (sy * imgW + sx) * channels;

			if (channels == 4 && p[3] == 0)
			{
				continue;
			}

			const int px0 = dstX + x * scale;

			if (px0 + scale <= 0 || px0 >= winW)
			{
				continue;
			}

			for (int oy = 0; oy < scale; ++oy)
			{
				const int py = py0 + oy;

				if (py < 0 || py >= winH)
				{
					continue;
				}

				for (int ox = 0; ox < scale; ++ox)
				{
					const int px = px0 + ox;

					if (px >= 0 && px < winW)
					{
						canvas.draw(px, py, const_cast<unsigned char*>(p));
					}
				}
			}
		}
	}
}
