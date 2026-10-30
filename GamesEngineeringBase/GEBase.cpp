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
#include "ArrowEntity.h" // Include the archer projectiles
#include "TileMapEntity.h" // Include the tile map (background system)
#include "Font5x7.h" // Include the bitmap font used for the FPS counter
#include "Hud.h" // Include the heart health display

// -----------------------------------------------------------

#include <filesystem>
#include <windows.h>
#include <algorithm>

// -----------------------------------------------------------

static const int kNinjaCount = 7;

// Minimum distance between enemy feet, used by the separation pass
static const float kNinjaGap = 40.0f;

// Enemy types: folder, sprite prefix, hit points, speed, aggro radius, ranged,
// spawn feet position. Yellow: fast, weak melee. Blue: slower, tougher melee.
// Green: archer, very slow, 2 hp, shoots when the player is in range.
struct NinjaSpawnDef
{
	const char* folder;
	const char* prefix;
	int hp;
	float speed;
	float aggro;
	bool ranged;
	float x;
	float y;
};

static const NinjaSpawnDef kNinjaSpawns[kNinjaCount] = {
	{ "..\\Resources\\YellowNinja", "yellowNinja", 3, 80.0f, 420.0f, false,  620.0f,  690.0f },
	{ "..\\Resources\\YellowNinja", "yellowNinja", 3, 80.0f, 420.0f, false,  420.0f,  620.0f },
	{ "..\\Resources\\YellowNinja", "yellowNinja", 3, 80.0f, 420.0f, false, 1120.0f,  620.0f },
	{ "..\\Resources\\BlueNinja",   "BlueNinja",   4, 70.0f, 420.0f, false,  900.0f,  900.0f },
	{ "..\\Resources\\BlueNinja",   "BlueNinja",   4, 70.0f, 420.0f, false,  250.0f, 1050.0f },
	{ "..\\Resources\\GreenNinja",  "GreenNinja",  2, 55.0f, 620.0f, true,  1250.0f,  480.0f },
	{ "..\\Resources\\GreenNinja",  "GreenNinja",  2, 55.0f, 620.0f, true,   350.0f,  480.0f }
};

static const int kArrowCount = 8;
static const float kArrowSpeed = 260.0f;

// One entry per sprite that has to be drawn with depth sorting
struct DrawItem
{
	float baseY;
	int kind;	// 0 = map prop, 1 = enemy, 2 = hero, 3 = arrow
	int index;
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
		ninjas[i].Reset({ kNinjaSpawns[i].x, kNinjaSpawns[i].y });
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

// Overlays every blocked tile in red (debug view for tuning collisions)
static void DrawBlockedOverlay(const TileMapEntity& world, GamesEngineeringBase::Window& canvas)
{
	const int camX = static_cast<int>(game::camera.x_);
	const int camY = static_cast<int>(game::camera.y_);
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());
	const int tile = TileMapEntity::kTile;

	const int firstX = camX / tile;
	const int firstY = camY / tile;

	for (int ty = firstY; ty <= (camY + winH) / tile; ++ty)
	{
		for (int tx = firstX; tx <= (camX + winW) / tile; ++tx)
		{
			if (!world.IsTileBlocked(tx, ty))
			{
				continue;
			}

			const int x0 = tx * tile - camX;
			const int y0 = ty * tile - camY;

			for (int y = 0; y < tile; ++y)
			{
				const int py = y0 + y;

				if (py < 0 || py >= winH)
				{
					continue;
				}

				for (int x = 0; x < tile; ++x)
				{
					const int px = x0 + x;

					if (px < 0 || px >= winW)
					{
						continue;
					}

					unsigned char* destination = canvas.backBuffer()
						+ (static_cast<size_t>(py) * winW + px) * 3;

					destination[0] = static_cast<unsigned char>((destination[0] * 155 + 255 * 100) / 255);
					destination[1] = static_cast<unsigned char>((destination[1] * 155 + 40 * 100) / 255);
					destination[2] = static_cast<unsigned char>((destination[2] * 155 + 40 * 100) / 255);
				}
			}
		}
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

	// Three yellow ninjas (3 hp, fast), two blue (4 hp, slower) and two green
	// archers (2 hp, very slow, shoot arrows)
	NinjaEntity ninjas[kNinjaCount];

	for (int i = 0; i < kNinjaCount; ++i)
	{
		const NinjaSpawnDef& def = kNinjaSpawns[i];

		ninjas[i].Init(def.folder, def.prefix, def.hp, def.speed, def.aggro, def.ranged,
			{ def.x, def.y });
	}

	ArrowEntity arrows[kArrowCount];
	int nextArrow = 0;

	for (int i = 0; i < kArrowCount; ++i)
	{
		arrows[i].Init("..\\Resources\\GreenNinja\\GreenNinja - arrow.png");
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

		// Enemies: chase the player and swing when close enough
		if (!world.IsInfinite())
		{
			float heroLeft = 0.0f, heroTop = 0.0f, heroRight = 0.0f, heroBottom = 0.0f;
			hero.BodyBox(heroLeft, heroTop, heroRight, heroBottom);

			for (int i = 0; i < kNinjaCount; ++i)
			{
				ninjas[i].Step(delta_time, hero.GetPosition(), world);

				// Archers release an arrow part way through the attack animation
				if (ninjas[i].ConsumeShot())
				{
					const Vec2f from{ ninjas[i].GetPosition().x_,
									  ninjas[i].GetPosition().y_ - 34.0f };

					Vec2f aim{ hero.GetPosition().x_ - from.x_,
							   (hero.GetPosition().y_ - 18.0f) - from.y_ };

					const float length = sqrtf(aim.x_ * aim.x_ + aim.y_ * aim.y_);

					if (length > 0.001f)
					{
						aim.x_ /= length;
						aim.y_ /= length;
					}

					arrows[nextArrow].Spawn(from, aim, kArrowSpeed, 1);
					nextArrow = (nextArrow + 1) % kArrowCount;
				}

				// Archers damage through their arrows only, not with the swing
				if (ninjas[i].IsDead() || hero.IsDead() || !ninjas[i].IsAttackActive()
					|| ninjas[i].IsRanged())
				{
					continue;
				}

				float al = 0.0f, at = 0.0f, ar = 0.0f, ab = 0.0f;
				ninjas[i].AttackBox(al, at, ar, ab);

				const bool overlap = !(ar < heroLeft || al > heroRight || ab < heroTop || at > heroBottom);

				if (overlap && ninjas[i].TryDamagePlayer())
				{
					hero.TakeDamage(1);
				}
			}

			// Arrows ignore terrain, they only stop on the player or when they
			// run out of lifetime
			for (int i = 0; i < kArrowCount; ++i)
			{
				arrows[i].Update(delta_time);

				if (!arrows[i].IsActive() || hero.IsDead())
				{
					continue;
				}

				float al = 0.0f, at = 0.0f, ar = 0.0f, ab = 0.0f;
				arrows[i].HurtBox(al, at, ar, ab);

				const bool overlap = !(ar < heroLeft || al > heroRight || ab < heroTop || at > heroBottom);

				if (overlap)
				{
					hero.TakeDamage(arrows[i].Damage());
					arrows[i].Deactivate();
				}
			}

			// Simple separation so the enemies do not stack on each other
			for (int a = 0; a < kNinjaCount; ++a)
			{
				for (int b = a + 1; b < kNinjaCount; ++b)
				{
					if (ninjas[a].IsDead() || ninjas[b].IsDead())
					{
						continue;
					}

					const Vec2f pa = ninjas[a].GetPosition();
					const Vec2f pb = ninjas[b].GetPosition();

					const float dx = pb.x_ - pa.x_;
					const float dy = pb.y_ - pa.y_;
					const float distance = sqrtf(dx * dx + dy * dy);

					if (distance < 0.001f || distance >= kNinjaGap)
					{
						continue;
					}

					const float push = (kNinjaGap - distance) * 0.5f;
					const float nx = dx / distance;
					const float ny = dy / distance;

					// Pushing apart must not shove an enemy inside a wall
					float bl = 0.0f, bt = 0.0f, br = 0.0f, bb = 0.0f;

					ninjas[a].SetPosition({ pa.x_ - nx * push, pa.y_ - ny * push });
					ninjas[a].BodyBox(bl, bt, br, bb);

					if (world.IsBoxBlocked(bl, bt, br, bb))
					{
						ninjas[a].SetPosition(pa);
					}

					ninjas[b].SetPosition({ pb.x_ + nx * push, pb.y_ + ny * push });
					ninjas[b].BodyBox(bl, bt, br, bb);

					if (world.IsBoxBlocked(bl, bt, br, bb))
					{
						ninjas[b].SetPosition(pb);
					}
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

		// Terrain first, then props and every actor in one list sorted by the
		// line their feet stand on. That is what makes enemies and the hero
		// hide behind roofs and trees correctly.
		world.DrawTerrain(canvas);

		DrawItem drawList[200];
		int drawCount = 0;

		if (!world.IsInfinite())
		{
			for (int i = 0; i < world.PropCount(); ++i)
			{
				drawList[drawCount].baseY = static_cast<float>(world.PropBaseY(i));
				drawList[drawCount].kind = 0;
				drawList[drawCount].index = i;
				++drawCount;
			}

			for (int i = 0; i < kNinjaCount; ++i)
			{
				drawList[drawCount].baseY = ninjas[i].GetPosition().y_;
				drawList[drawCount].kind = 1;
				drawList[drawCount].index = i;
				++drawCount;
			}

			for (int i = 0; i < kArrowCount; ++i)
			{
				if (!arrows[i].IsActive())
				{
					continue;
				}

				drawList[drawCount].baseY = arrows[i].Position().y_;
				drawList[drawCount].kind = 3;
				drawList[drawCount].index = i;
				++drawCount;
			}
		}

		drawList[drawCount].baseY = hero.GetPosition().y_;
		drawList[drawCount].kind = 2;
		drawList[drawCount].index = 0;
		++drawCount;

		// Insertion sort on the base line (small list, runs every frame)
		for (int i = 1; i < drawCount; ++i)
		{
			const DrawItem key = drawList[i];
			int j = i - 1;

			while (j >= 0 && drawList[j].baseY > key.baseY)
			{
				drawList[j + 1] = drawList[j];
				--j;
			}
			drawList[j + 1] = key;
		}

		for (int i = 0; i < drawCount; ++i)
		{
			const DrawItem& item = drawList[i];

			if (item.kind == 0)
			{
				world.DrawPropAt(canvas, item.index);
			}
			else if (item.kind == 1)
			{
				ninjas[item.index].Draw(canvas);
			}
			else if (item.kind == 3)
			{
				arrows[item.index].Draw(canvas);
			}
			else
			{
				// The hurt animation is the damage feedback, no need to blink
				// (blinking also made the hero vanish on screenshots)
				hero.Draw(canvas);
			}
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

		// Hold G to see every blocked tile
		if (canvas.keyPressed('G'))
		{
			DrawBlockedOverlay(world, canvas);
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
