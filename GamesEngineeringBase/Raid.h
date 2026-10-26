#pragma once

#include <iostream>
#include <string>
#include <random>

#include "GameEntity.h"

class WorldEntity : public VisualEntity {
public:
	
	static constexpr int kTileType = 6;
	static constexpr int kWorldHeight = 1024;
	static constexpr int kTileHeight = 64;

	WorldEntity(const Vec2f& pos) : VisualEntity(pos) {
		
		
		for (int i = 0; i < kTileType; i++)
		{
			std::string image_filename = "..\\Resources\\tile_" + std::to_string(i) + ".png";
			std::cout << "Loading tile image: " << image_filename << std::endl;
			tiles[i].load(image_filename);
		}

		std::mt19937 random{ std::random_device{}() };
		std::uniform_int_distribution<int> distribution(0, kTileType - 1);

		for (int n = 0; n < kWorldHeight; n++)
		{
			world[n] = 0; // Initialize the world with default tile type (0)
			world[n] = pick(random);
		}

	}

	virtual void Update(float delta_time) override {
		// WorldEntity might not have any update logic for now
	}

	virtual void Draw(GamesEngineeringBase::Window& canvas) override {
		
		const int scroll = static_cast<int>(scroll_offset);
		scroll_offset += static_cast<int>(scroll_speed * delta_time); // Adjust the scroll speed as needed

		const int view_height = canvas.getHeight();
		const int tile_height = tiles[0].getHeight();
		const int first = scroll / tile_height;
		const int last = (scroll + view_height -1) / tile_height;
		const float fit = static_cast<float>(view_height) / tile_height;

		
		for (int n = first; n < last; n++)
		{
			int top_ = scroll + view_height - ( n+1 ) * tile_height;
			
			Renderer_draw_image(canvas, 
				{0.0f, static_cast<float>(top_)}, 
				tiles[world[n]]);
		}
	}

	void SetScrollSpeed(float speed) {
		scroll_speed = speed;
	}

private:
	// Add any private members or methods here if needed
	GamesEngineeringBase::Image tiles[kTileType];
	int world[kWorldHeight];
};