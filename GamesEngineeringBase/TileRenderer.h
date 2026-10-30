#pragma once

#include "GamesEngineeringBase.h"
#include <cstddef>

// ---------------------------------------------------------------------------
// Low level blitting helpers used by the tile map.
//
// The framework only offers full-image drawing (Renderer_draw_image), so a tile
// map needs to copy 32x32 regions out of a tileset. These helpers do that and
// honour the source alpha channel, which is what makes transparent tiles and
// sprites work. Partial alpha is blended with whatever is already in the back
// buffer (the UI uses faded sprites, e.g. empty hearts).
// ---------------------------------------------------------------------------

inline void Renderer_blend_pixel(GamesEngineeringBase::Window& canvas,
	int x, int y, const unsigned char* pixel, int alpha)
{
	if (alpha <= 0)
	{
		return;
	}

	if (alpha >= 255)
	{
		canvas.draw(x, y, const_cast<unsigned char*>(pixel));
		return;
	}

	unsigned char* destination = canvas.backBuffer()
		+ (static_cast<size_t>(y) * canvas.getWidth() + static_cast<size_t>(x)) * 3;

	const int inverse = 255 - alpha;
	destination[0] = static_cast<unsigned char>((pixel[0] * alpha + destination[0] * inverse) / 255);
	destination[1] = static_cast<unsigned char>((pixel[1] * alpha + destination[1] * inverse) / 255);
	destination[2] = static_cast<unsigned char>((pixel[2] * alpha + destination[2] * inverse) / 255);
}

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

			Renderer_blend_pixel(canvas, dstX + x, dstY + y, p,
				channels == 4 ? p[3] : 255);
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
						Renderer_blend_pixel(canvas, px, py, p,
							channels == 4 ? p[3] : 255);
					}
				}
			}
		}
	}
}

// Draws a sprite rotated around its centre (used by the arrows, whose art is
// drawn pointing right but which can fly in any direction). Nearest neighbour,
// no smoothing, so it keeps the pixel look.
inline void Renderer_blit_rotated(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& image,
	int srcX, int srcY, int srcW, int srcH,
	float centerX, float centerY, float angle, float scale)
{
	const unsigned char* src = image.data;

	if (src == nullptr || srcW <= 0 || srcH <= 0 || scale <= 0.0f)
	{
		return;
	}

	const int channels = static_cast<int>(image.channels);
	const int imgW = static_cast<int>(image.width);
	const int imgH = static_cast<int>(image.height);
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());

	const int halfW = static_cast<int>(srcW * scale * 0.5f) + 1;
	const int halfH = static_cast<int>(srcH * scale * 0.5f) + 1;

	const float cosA = cosf(angle);
	const float sinA = sinf(angle);

	const float srcCenterX = srcW * 0.5f;
	const float srcCenterY = srcH * 0.5f;

	for (int dy = -halfH; dy <= halfH; ++dy)
	{
		const int py = static_cast<int>(centerY) + dy;

		if (py < 0 || py >= winH)
		{
			continue;
		}

		for (int dx = -halfW; dx <= halfW; ++dx)
		{
			const int px = static_cast<int>(centerX) + dx;

			if (px < 0 || px >= winW)
			{
				continue;
			}

			// Inverse rotation back into sprite space
			const float sx = (dx * cosA + dy * sinA) / scale + srcCenterX;
			const float sy = (-dx * sinA + dy * cosA) / scale + srcCenterY;

			const int texelX = static_cast<int>(floorf(sx));
			const int texelY = static_cast<int>(floorf(sy));

			if (texelX < 0 || texelY < 0 || texelX >= srcW || texelY >= srcH)
			{
				continue;
			}

			const unsigned char* p = src
				+ ((srcY + texelY) * imgW + (srcX + texelX)) * channels;

			if (channels == 4 && p[3] == 0)
			{
				continue;
			}

			Renderer_blend_pixel(canvas, px, py, p, channels == 4 ? p[3] : 255);
		}
	}
}
