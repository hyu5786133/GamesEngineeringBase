#pragma once

#include <iostream>
#include <string>
#include <random>

#include "GameEntity.h"

// ---------------------------------------------------------------------------
// Old file from tutorial in class, kept for reference. 
// The new background system is in TileMapEntity.
// ---------------------------------------------------------------------------

class WorldEntity : public VisualEntity {
public:

	static constexpr int kTileType = 6;
	static constexpr int kWorldHeight = 1024;

	WorldEntity(const Vec2f& pos) : VisualEntity(pos)
	{
		for (int i = 0; i < kTileType; i++)
		{
			const std::string image_filename = "..\\Resources\\RiverRaidAssets\\tile_" + std::to_string(i) + ".png";
			const std::string alpha_filename = "..\\Resources\\RiverRaidAssets\\alpha_" + std::to_string(i) + ".png";

			if (!tiles[i].load(image_filename))
			{
				std::cerr << "[WorldEntity] Failed to load tile: " << image_filename << std::endl;
			}
			if (!alphas[i].load(alpha_filename))
			{
				std::cerr << "[WorldEntity] Failed to load alpha: " << alpha_filename << std::endl;
			}
		}

		std::mt19937 random{ std::random_device{}() };
		std::uniform_int_distribution<int> pick(0, kTileType - 1);

		for (int n = 0; n < kWorldHeight; n++)
		{
			world[n] = pick(random);
		}
	}

	virtual void Update(float delta_time) override {
		// scroll_speed is in pixels per second, so the scroll rate is frame rate independent
		scroll_offset += scroll_speed * delta_time;
	}

	virtual void Draw(GamesEngineeringBase::Window& canvas) override {

		const int tile_height = static_cast<int>(tiles[0].height);
		if (tile_height <= 0)
		{
			return; // tiles are not loaded: nothing to draw, and do not divide by zero
		}

		const int scroll = static_cast<int>(scroll_offset);
		const int view_height = static_cast<int>(canvas.getHeight());
		const int first = scroll / tile_height;
		const int last = (scroll + view_height - 1) / tile_height;

		for (int n = first; n <= last; n++)
		{
			const int top = scroll + view_height - (n + 1) * tile_height;

			// Holding SPACE shows the alpha masks instead of the tile artwork
			Renderer_draw_image(canvas,
				{ 0.0f, static_cast<float>(top) },
				show_alphas_ ? alphas[TileType(n)] : tiles[TileType(n)]);
		}

		if (show_alphas_)
		{
			DrawCollisionLine(canvas, static_cast<float>(view_height / 2));
		}
	}

	// Water/land test: map a screen point back to a world row, then sample the
	// alpha mask of that tile. Black = land, white = water.
	bool IsLand(GamesEngineeringBase::Window& canvas, const Vec2f& screen_point) const {

		const int tile_height = static_cast<int>(tiles[0].height);
		if (tile_height <= 0)
		{
			return false;
		}
		if (screen_point.x_ < 0.0f || screen_point.x_ >= static_cast<float>(canvas.getWidth()))
		{
			return false;
		}

		const int view_height = static_cast<int>(canvas.getHeight());
		const int world_y = static_cast<int>(scroll_offset) + view_height
							- 1 - static_cast<int>(screen_point.y_);

		if (world_y < 0)
		{
			return false;
		}

		const int tile = world_y / tile_height;
		const int row = tile_height - 1 - (world_y % tile_height);

		const GamesEngineeringBase::Image& alpha = alphas[TileType(tile)];
		if (alpha.data == nullptr)
		{
			return false;
		}

		return alpha.at(static_cast<unsigned int>(screen_point.x_),
						static_cast<unsigned int>(row), 0) < 128;
	}

	int TileType(int tile_index) const {
		const int index = ((tile_index % kWorldHeight) + kWorldHeight) % kWorldHeight;
		return world[index];
	}

	// Debug overlay: green where the point is land, red where it is water
	void DrawCollisionLine(GamesEngineeringBase::Window& canvas, float y) {

		const int line_y = static_cast<int>(y);
		if (line_y < 0 || line_y >= static_cast<int>(canvas.getHeight()))
		{
			return;
		}

		for (int x = 0; x < static_cast<int>(canvas.getWidth()); x++)
		{
			const bool land = IsLand(canvas, { static_cast<float>(x), static_cast<float>(line_y) });

			if (land)
			{
				canvas.draw(x, line_y, 0, 255, 0);
			}
			else
			{
				canvas.draw(x, line_y, 255, 0, 0);
			}
		}
	}

	void SetScrollSpeed(float speed) {
		scroll_speed = speed;
	}

	void SetShowAlphas(bool show) {
		show_alphas_ = show;
	}

private:
	GamesEngineeringBase::Image tiles[kTileType];
	GamesEngineeringBase::Image alphas[kTileType];

	int world[kWorldHeight];
	float scroll_offset = 5000.0f;
	float scroll_speed = 60.0f; // pixels per second
	bool show_alphas_ = false;
};
