#pragma once

#include "GamesEngineeringBase.h"

// ---------------------------------------------------------------------------
// Tiny 5x7 bitmap font. The framework has no text support and the FPS counter
// only needs letters, digits, '.' and ':', so a small table is enough.
// Each glyph is 7 rows of 5 bits, bit 4 is the leftmost pixel.
// ---------------------------------------------------------------------------

struct BitmapGlyph
{
	char c;
	unsigned char rows[7];
};

inline const BitmapGlyph kBitmapGlyphs[] = {
	{ '0', { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 } },
	{ '1', { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 } },
	{ '2', { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 } },
	{ '3', { 0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110 } },
	{ '4', { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 } },
	{ '5', { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 } },
	{ '6', { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 } },
	{ '7', { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 } },
	{ '8', { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 } },
	{ '9', { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 } },
	{ 'F', { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000 } },
	{ 'P', { 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000 } },
	{ 'S', { 0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b10001, 0b01110 } },
	{ 'm', { 0b00000, 0b00000, 0b11010, 0b10101, 0b10101, 0b10001, 0b10001 } },
	{ 's', { 0b00000, 0b00000, 0b01111, 0b10000, 0b01110, 0b00001, 0b11110 } },
	{ '.', { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00110, 0b00110 } },
	{ ':', { 0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000 } },
	{ ' ', { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000 } }
};

inline const BitmapGlyph* FindBitmapGlyph(char c)
{
	const int count = static_cast<int>(sizeof(kBitmapGlyphs) / sizeof(kBitmapGlyphs[0]));

	for (int i = 0; i < count; ++i)
	{
		if (kBitmapGlyphs[i].c == c)
		{
			return &kBitmapGlyphs[i];
		}
	}
	return nullptr;
}

inline void DrawBitmapText(GamesEngineeringBase::Window& canvas, int x, int y,
	const char* text, int scale,
	unsigned char r, unsigned char g, unsigned char b)
{
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());

	for (; *text != '\0'; ++text)
	{
		const BitmapGlyph* glyph = FindBitmapGlyph(*text);

		if (glyph == nullptr)
		{
			x += 4 * scale;
			continue;
		}

		for (int row = 0; row < 7; ++row)
		{
			for (int col = 0; col < 5; ++col)
			{
				if ((glyph->rows[row] & (1 << (4 - col))) == 0)
				{
					continue;
				}

				for (int sy = 0; sy < scale; ++sy)
				{
					for (int sx = 0; sx < scale; ++sx)
					{
						const int px = x + col * scale + sx;
						const int py = y + row * scale + sy;

						if (px >= 0 && px < winW && py >= 0 && py < winH)
						{
							canvas.draw(px, py, r, g, b);
						}
					}
				}
			}
		}

		x += 6 * scale;
	}
}
