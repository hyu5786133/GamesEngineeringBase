#include <iostream>
#include "GamesEngineeringBase.h" // Include the GamesEngineeringBase header
#include <string>
#include <stdexcept>
#include <random>
#include <cstdio>

// -----------------------------------------------------------

#include "Entity.h" // Include the Entity header
#include "GameEntity.h" // Include the GameEntity header
#include "TileMapEntity.h" // Include the tile map (background system)
#include "Font5x7.h" // Include the bitmap font used for the FPS counter

// -----------------------------------------------------------

#include <filesystem>
#include <windows.h>
#include <algorithm>

// -----------------------------------------------------------

// Places the hero so its feet stand on the middle of the spawn tile
static void PlaceHeroOnSpawn(HeroEntity& hero, const TileMapEntity& world)
{
	int spawnX = 0;
	int spawnY = 0;
	world.SpawnTile(spawnX, spawnY);

	const float x = spawnX * TileMapEntity::kTile
		+ (TileMapEntity::kTile - hero.GetImageWidth()) / 2.0f;
	const float y = static_cast<float>((spawnY + 1) * TileMapEntity::kTile - hero.GetImageHeight());

	hero.SetPosition({ x, y });
}

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

	HeroEntity hero({ 0.0f, 0.0f });
	PlaceHeroOnSpawn(hero, world);

	float delta_time = timer.dt(); // Get the elapsed time since the last frame.

	bool running = true; // Variable to control the main loop's running state.

	// FPS / frame time, refreshed twice a second
	float fpsAccum = 0.0f;
	int fpsFrames = 0;
	int fpsValue = 0;
	float msValue = 0.0f;

	// Used to detect the moment '1' or '2' is pressed, not every frame they are held
	bool keyOneDown = false;
	bool keyTwoDown = false;

	while (running)
	{
		delta_time = timer.dt();

		if (canvas.keyPressed(VK_ESCAPE)) break;

		// 1 = fixed village map, 2 = endless procedural map
		const bool keyOne = canvas.keyPressed('1');
		const bool keyTwo = canvas.keyPressed('2');

		if (keyOne && !keyOneDown)
		{
			if (world.LoadMap("..\\Resources\\village.map"))
			{
				PlaceHeroOnSpawn(hero, world);
				std::cerr << "Loaded village.map\n";
			}
		}

		if (keyTwo && !keyTwoDown)
		{
			world.MakeInfinite(20261029);
			std::cerr << "Endless map enabled\n";
		}

		keyOneDown = keyOne;
		keyTwoDown = keyTwo;

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

		// Keep the hero inside the map, the endless map has no edges
		if (!world.IsInfinite())
		{
			const float maxX = static_cast<float>(world.Width() * TileMapEntity::kTile) - static_cast<float>(hero.GetImageWidth());
			const float maxY = static_cast<float>(world.Height() * TileMapEntity::kTile) - static_cast<float>(hero.GetImageHeight());
			hero.SetPosition({ std::clamp(hero.GetPosition().x_, 0.0f, maxX),
							   std::clamp(hero.GetPosition().y_, 0.0f, maxY) });
		}

		// Camera follows the hero, clamped to the map bounds for fixed maps
		const Vec2f hero_center{ hero.GetPosition().x_ + hero.GetImageWidth() / 2.0f,
								 hero.GetPosition().y_ + hero.GetImageHeight() / 2.0f };
		world.FollowCamera(hero_center, canvas);
		game::camera = world.Camera();

		world.Draw(canvas);
		hero.Draw(canvas);

		// FPS overlay: black shadow first so it stays readable on any terrain
		fpsAccum += delta_time;
		++fpsFrames;

		if (fpsAccum >= 0.5f)
		{
			fpsValue = static_cast<int>(fpsFrames / fpsAccum + 0.5f);
			msValue = fpsAccum * 1000.0f / static_cast<float>(fpsFrames);
			fpsAccum = 0.0f;
			fpsFrames = 0;
		}

		char fpsText[32];
		std::snprintf(fpsText, sizeof(fpsText), "FPS %d  %.1fms", fpsValue, msValue);

		DrawBitmapText(canvas, 9, 9, fpsText, 2, 0, 0, 0);
		DrawBitmapText(canvas, 8, 8, fpsText, 2, 255, 255, 255);

		canvas.present();
	}
	return 0;
}
