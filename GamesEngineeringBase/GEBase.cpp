#include <iostream>
#include "GamesEngineeringBase.h" // Include the GamesEngineeringBase header
#include <string>
#include <stdexcept>
#include <random>

// -----------------------------------------------------------

#include "Entity.h" // Include the Entity header
#include "GameEntity.h" // Include the GameEntity header
#include "TileMapEntity.h" // Include the tile map (new background system)

// -----------------------------------------------------------

#include <filesystem>
#include <windows.h>
#include <algorithm>

// -----------------------------------------------------------

int main() {

	// Debug: print the working directory so a broken resource path is easy to spot
	wchar_t cwd[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, cwd);
	std::wcerr << L"CWD = " << cwd << L"\n";

	// Create a timer object to measure time intervals.
	GamesEngineeringBase::Timer timer;

	// Create a canvas window with dimensions 1024x768 and title "Tiles"
	GamesEngineeringBase::Window canvas;
	canvas.create(1024, 768, "Tiles");

	// Load the village layout from the map file (see MapFile.h for the format)
	TileMapEntity world;

	if (!world.LoadMap("..\\Resources\\village.map"))
	{
		std::cerr << "Failed to load village.map" << std::endl;
		return 1;
	}

	// Spawn on the tile the map file specifies, standing on the bottom of it
	HeroEntity hero({ 0.0f, 0.0f });

	int spawnX = 0;
	int spawnY = 0;
	world.SpawnTile(spawnX, spawnY);

	const float spawnPixelX = spawnX * TileMapEntity::kTile
		+ (TileMapEntity::kTile - hero.GetImageWidth()) / 2.0f;
	const float spawnPixelY = static_cast<float>((spawnY + 1) * TileMapEntity::kTile
		- hero.GetImageHeight());

	hero.SetPosition({ spawnPixelX, spawnPixelY });

	float delta_time = timer.dt(); // Get the elapsed time since the last frame.

	bool running = true; // Variable to control the main loop's running state.

	while (running)
	{
		delta_time = timer.dt();

		if (canvas.keyPressed(VK_ESCAPE)) break;

		canvas.clear(0, 0, 0);

		Vec2f input{ 0, 0 };
		if (canvas.keyPressed('W')) input.y_ = -2.0f;
		if (canvas.keyPressed('S')) input.y_ = 2.0f;
		if (canvas.keyPressed('A')) input.x_ = -2.0f;
		if (canvas.keyPressed('D')) input.x_ = 2.0f;

		hero.SetInputAxis(input);

		const Vec2f previous_hero_position = hero.GetPosition();
		hero.Update(delta_time);
		world.Update(delta_time);

		// Impassable tiles (water, houses, tree trunks) block the hero
		const Vec2f hero_feet{ hero.GetPosition().x_ + hero.GetImageWidth() / 2.0f,
							   hero.GetPosition().y_ + hero.GetImageHeight() - 1.0f };
		if (world.IsBlocked(static_cast<int>(hero_feet.x_), static_cast<int>(hero_feet.y_)))
		{
			hero.SetPosition(previous_hero_position);
		}

		// Keep the hero inside the world
		const float maxX = static_cast<float>(world.Width() * TileMapEntity::kTile) - static_cast<float>(hero.GetImageWidth());
		const float maxY = static_cast<float>(world.Height() * TileMapEntity::kTile) - static_cast<float>(hero.GetImageHeight());
		hero.SetPosition({ std::clamp(hero.GetPosition().x_, 0.0f, maxX),
						   std::clamp(hero.GetPosition().y_, 0.0f, maxY) });

		// Camera follows the hero, clamped to the world bounds
		const Vec2f hero_center{ hero.GetPosition().x_ + hero.GetImageWidth() / 2.0f,
								 hero.GetPosition().y_ + hero.GetImageHeight() / 2.0f };
		world.FollowCamera(hero_center, canvas);
		game::camera = world.Camera();

		world.Draw(canvas);
		hero.Draw(canvas);

		canvas.present();
	}
	return 0;
}
