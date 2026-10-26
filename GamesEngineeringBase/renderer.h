#pragma once

#include "shapes.h"


void Renderer_draw_image(GamesEngineeringBase::Window& canvas,
    Vec2f position,
    const GamesEngineeringBase::Image& image)
{

    if (image.data == nullptr)
    {
        return;
    }

    // std::cout << "Drawing image at position (" << position.x_ << ", " << position.y_ << ")" << std::endl;

    for (unsigned int row = 0; row < image.height; ++row)
    {
        const int y = position.y_ + static_cast<int>(row);

        if (y < 0 || y >= canvas.getHeight())
        {
            // std::cout << "Skipping row " << row << " as it is outside the window bounds." << std::endl;
            continue;
        }

        for (unsigned int col = 0; col < image.width; ++col)
        {
            const int x = position.x_ + static_cast<int>(col);

            if (x < 0 || x >= canvas.getWidth())
            {
                // std::cout << "Skipping column " << col << " as it is outside the window bounds." << std::endl;  
                continue;
            }

            const unsigned int pixelIndex = (row * image.width + col) * image.channels;

            if (image.channels == 3 || image.data[pixelIndex + 3] != 0)
            {
                canvas.draw(x, y, &image.data[pixelIndex]);
            }
        }
    }
}


void Renderer_drawCircle(GamesEngineeringBase::Window& canvas,
    const int cx, const int cy,
    const int rd,
    const int r = 255, const int g = 255, const int b = 0,
    const int step = 1) {
    const int r2 = rd * rd;

    const int width = canvas.getWidth();
    const int height = canvas.getHeight();

    auto plot = [&](const int px, const int py) {
        if (px >= 0 && px < width && py >= 0 && py < height)
            canvas.draw(px, py, r, g, b);
        };

    // Walk only one octant (x up to rd/sqrt(2), where the slope is <= 1) so
    // consecutive pixels never skip a row, then mirror into all 8 octants.
    const int limit = static_cast<int>(rd * 0.7071068f);
    for (int x = 0; x <= limit; x += step) {
        const int y = static_cast<int>(sqrtf(static_cast<float>(r2 - x * x)) + 0.5f);

        plot(cx + x, cy + y);
        plot(cx - x, cy + y);
        plot(cx + x, cy - y);
        plot(cx - x, cy - y);
        plot(cx + y, cy + x);
        plot(cx - y, cy + x);
        plot(cx + y, cy - x);
        plot(cx - y, cy - x);
    }
}