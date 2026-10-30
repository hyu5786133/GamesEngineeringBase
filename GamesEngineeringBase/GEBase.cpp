#include <iostream>
#include "GamesEngineeringBase.h" // Include the GamesEngineeringBase header
#include <string>
#include <stdexcept>
#include <random>
#include <cstdio>

// -----------------------------------------------------------

#include "Entity.h" // Include the Entity header
#include "SamuraiEntity.h" // Include the player character
#include "NinjaEntity.h" // Include the melee enemies
#include "TileMapEntity.h" // Include the tile map (background system)
#include "Font5x7.h" // Include the bitmap font used for the FPS counter
#include "Hud.h" // Include the heart health display

// -----------------------------------------------------------

#include <filesystem>
#include <windows.h>
#include <algorithm>

// -----------------------------------------------------------

static const int kNinjaCount = 3;

// Village spawn points for the enemies
static const Vec2f kNinjaSpawn[kNinjaCount] = {
	{ 540.0f, 470.0f },
	{ 330.0f, 550.0f },
	{ 980.0f, 520.0f }
};

// -----------------------------------------------------------

// Places the hero so its feet stand on the middle of the spawn tile
static void PlaceHeroOnSpawn(SamuraiEntity& hero, const TileMapEntity& world)
{
	int spawnX = 0;
	int spawnY = 0;
	world.SpawnTile(spawnX, spawnY);

	const float x = spawnX * TileMapEntity::kTile + TileMapEntity::kTile * 0.5f;
	const float y = static_cast<float>((spawnY + 1) * TileMapEntity::kTile - 2);

	hero.SetPosition({ x, y });
	hero.Respawn();
}

static void ResetNinjas(NinjaEntity ninjas[kNinjaCount])
{
	for (int i = 0; i < kNinjaCount; ++i)
	{
		ninjas[i].Reset(kNinjaSpawn[i]);
	}
}

// Draws a world space rectangle as a 1px outline (debug view of a hit box)
static void DrawWorldBox(GamesEngineeringBase::Window& canvas,
	float left, float top, float right, float bottom,
	unsigned char r, unsigned char g, unsigned char b)
{
	const int x0 = static_cast<int>(left - game::camera.x_);
	const int x1 = static_cast<int>(right - game::camera.x_);
	const int y0 = static_cast<int>(top - game::camera.y_);
	const int y1 = static_cast<int>(bottom - game::camera.y_);

	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());

	for (int x = x0; x <= x1; ++x)
	{
		if (x < 0 || x >= winW) continue;
		if (y0 >= 0 && y0 < winH) canvas.draw(x, y0, r, g, b);
		if (y1 >= 0 && y1 < winH) canvas.draw(x, y1, r, g, b);
	}

	for (int y = y0; y <= y1; ++y)
	{
		if (y < 0 || y >= winH) continue;
		if (x0 >= 0 && x0 < winW) canvas.draw(x0, y, r, g, b);
		if (x1 >= 0 && x1 < winW) canvas.draw(x1, y, r, g, b);
	}
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

	// UI sheet used for the heart health display
	GamesEngineeringBase::Image uiSheet;

	if (!uiSheet.load("..\\Resources\\MediavelFull.png"))
	{
		std::cerr << "Failed to load MediavelFull.png" << std::endl;
	}

	SamuraiEntity hero({ 0.0f, 0.0f });
	PlaceHeroOnSpawn(hero, world);

	// Three yellow ninjas, each with 3 hit points
	NinjaEntity ninjas[kNinjaCount];

	for (int i = 0; i < kNinjaCount; ++i)
	{
		ninjas[i].Init("..\\Resources\\YellowNinja", "yellowNinja", 3, kNinjaSpawn[i]);
	}

	float delta_time = timer.dt(); // Get the elapsed time since the last frame.

	bool running = true; // Variable to control the main loop's running state.

	// FPS / frame time, refreshed twice a second
	float fpsAccum = 0.0f;
	int fpsFrames = 0;
	int fpsValue = 0;
	float msValue = 0.0f;

	int frameCounter = 0;

	// Edge detection for the one shot keys
	bool keyOneDown = false;
	bool keyTwoDown = false;
	bool attackDown = false;
	bool hurtDown = false;
	bool deathDown = false;
	bool respawnDown = false;

	while (running)
	{
		delta_time = timer.dt();
		++frameCounter;

		if (canvas.keyPressed(VK_ESCAPE)) break;

		// 1 = fixed village map, 2 = endless procedural map
		const bool keyOne = canvas.keyPressed('1');
		const bool keyTwo = canvas.keyPressed('2');

		if (keyOne && !keyOneDown)
		{
			if (world.LoadMap("..\\Resources\\village.map"))
			{
				PlaceHeroOnSpawn(hero, world);
				ResetNinjas(ninjas);
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

		// Actions: J attack, R respawn (H/K are still handy debug triggers)
		const bool attackKey = canvas.keyPressed('J');
		const bool hurtKey = canvas.keyPressed('H');
		const bool deathKey = canvas.keyPressed('K');
		const bool respawnKey = canvas.keyPressed('R');

		if (attackKey && !attackDown) { hero.Attack(); }
		if (hurtKey && !hurtDown) { hero.TakeDamage(1); }
		if (deathKey && !deathDown) { hero.Die(); }

		if (respawnKey && !respawnDown)
		{
			PlaceHeroOnSpawn(hero, world);
			ResetNinjas(ninjas);
		}

		attackDown = attackKey;
		hurtDown = hurtKey;
		deathDown = deathKey;
		respawnDown = respawnKey;

		canvas.clear(0, 0, 0);

		Vec2f input{ 0, 0 };
		if (canvas.keyPressed('W')) input.y_ = -1.0f;
		if (canvas.keyPressed('S')) input.y_ = 1.0f;
		if (canvas.keyPressed('A')) input.x_ = -1.0f;
		if (canvas.keyPressed('D')) input.x_ = 1.0f;

		hero.SetInputAxis(input);
		hero.SetRunning(canvas.keyPressed(VK_SHIFT));

		const Vec2f previous_feet = hero.GetPosition();
		hero.Update(delta_time);
		world.Update(delta_time);

		// The whole body box collides with impassable tiles
		float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
		hero.BodyBox(left, top, right, bottom);

		if (world.IsBoxBlocked(left, top, right, bottom))
		{
			hero.SetPosition(previous_feet);
		}

		// Keep the hero body inside the map, the endless map has no edges
		if (!world.IsInfinite())
		{
			const float halfWidth = SamuraiEntity::kBodyW * SamuraiEntity::kScale * 0.5f;
			const float bodyHeight = SamuraiEntity::kBodyH * SamuraiEntity::kScale;

			const float minX = halfWidth;
			const float maxX = static_cast<float>(world.Width() * TileMapEntity::kTile) - halfWidth;
			const float minY = bodyHeight;
			const float maxY = static_cast<float>(world.Height() * TileMapEntity::kTile);

			hero.SetPosition({ std::clamp(hero.GetPosition().x_, minX, maxX),
							   std::clamp(hero.GetPosition().y_, minY, maxY) });
		}

		// Enemies: chase the player, then damage on contact
		if (!world.IsInfinite())
		{
			float heroLeft = 0.0f, heroTop = 0.0f, heroRight = 0.0f, heroBottom = 0.0f;
			hero.BodyBox(heroLeft, heroTop, heroRight, heroBottom);

			for (int i = 0; i < kNinjaCount; ++i)
			{
				ninjas[i].Step(delta_time, hero.GetPosition(), world);

				if (ninjas[i].IsDead() || hero.IsDead())
				{
					continue;
				}

				float nl = 0.0f, nt = 0.0f, nr = 0.0f, nb = 0.0f;
				ninjas[i].BodyBox(nl, nt, nr, nb);

				const bool overlap = !(nr < heroLeft || nl > heroRight || nb < heroTop || nt > heroBottom);

				if (overlap && ninjas[i].TryContactDamage())
				{
					hero.TakeDamage(1);
				}
			}
		}

		// Melee hit detection: while the blade is out, damage anything the hit
		// box overlaps. Each swing can hit a target only once.
		bool attack_active = false;
		float attackLeft = 0.0f, attackTop = 0.0f, attackRight = 0.0f, attackBottom = 0.0f;

		if (hero.IsAttackActive())
		{
			attack_active = true;
			hero.AttackBox(attackLeft, attackTop, attackRight, attackBottom);

			if (!world.IsInfinite())
			{
				for (int i = 0; i < kNinjaCount; ++i)
				{
					ninjas[i].TryHit(hero.SwingId(), attackLeft, attackTop, attackRight, attackBottom);
				}
			}
		}

		// Camera follows the middle of the hero body
		world.FollowCamera(hero.BodyCentre(), canvas);
		game::camera = world.Camera();

		world.Draw(canvas);

		if (!world.IsInfinite())
		{
			for (int i = 0; i < kNinjaCount; ++i)
			{
				ninjas[i].Draw(canvas);
			}
		}

		// Blink the hero while he is invulnerable after a hit
		if (!hero.IsInvulnerable() || ((frameCounter / 4) % 2) == 0)
		{
			hero.Draw(canvas);
		}

		// Hold B to see the collision box, V to see the sword hit box
		if (canvas.keyPressed('B'))
		{
			hero.BodyBox(left, top, right, bottom);
			DrawWorldBox(canvas, left, top, right, bottom, 255, 0, 255);
		}

		if (canvas.keyPressed('V') && attack_active)
		{
			DrawWorldBox(canvas, attackLeft, attackTop, attackRight, attackBottom, 255, 255, 0);
		}

		// HUD: hearts under the frame counter
		DrawHearts(canvas, uiSheet, 8, 30, hero.Hp(), hero.MaxHp(), 2);

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
