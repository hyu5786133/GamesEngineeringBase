#include <iostream>
#include "GamesEngineeringBase.h" // Include the GamesEngineeringBase header
#include <string>
#include <stdexcept>
#include <random>
#include <cstdio>
#include <cstring>
#include <cmath>

// -----------------------------------------------------------

#include "Entity.h" // Include the Entity header
#include "SamuraiEntity.h" // Include the player character
#include "NinjaEntity.h" // Include the enemy types
#include "ArrowEntity.h" // Include the archer projectiles
#include "SpawnerEntity.h" // Include the destroyable objectives
#include "TileMapEntity.h" // Include the tile map (background system)
#include "Font5x7.h" // Include the bitmap font used for the HUD
#include "Hud.h" // Include the heart health display

// -----------------------------------------------------------

#include <filesystem>
#include <windows.h>
#include <algorithm>

// -----------------------------------------------------------

static const int kMaxEnemies = 16;
static const int kArrowCount = 8;
static const float kArrowSpeed = 260.0f;
static const float kNinjaGap = 40.0f;

enum { kProfileYellow = 0, kProfileBlue, kProfileGreen, kProfileCount };

enum GameState
{
	kTitle,
	kIntro,
	kCharSelect,
	kControls,
	kLevelSelect,
	kPlaying,
	kVictory,
	kDefeat
};

// Playable characters. Only the samurai is implemented for now; the other two
// slots are shown greyed out so the selection screen already has its shape.
struct CharacterDef
{
	const char* name;
	const char* role;
	const char* skill;
	bool locked;
};

static const int kCharacterCount = 3;

static const CharacterDef kCharacters[kCharacterCount] = {
	{ "SAMURAI", "MELEE",      "RUN",  false },
	{ "RANGED",  "ARCHER",     "LATER", true },
	{ "MAGE",    "SPELLCASTER", "LATER", true }
};

// Stages that can be picked at the end of the intro
struct LevelDef
{
	const char* name;
	const char* desc;
	bool infinite;
};

static const int kLevelCount = 2;

static const LevelDef kLevels[kLevelCount] = {
	{ "THE VILLAGE", "FIXED MAP  3 TOTEMS", false },
	{ "ENDLESS ROAD", "INFINITE MAP  SURVIVE", true }
};

// Enemies already in the level at start up, the objectives spawn the rest
struct InitialEnemy
{
	int profile;
	float x;
	float y;
};

static const int kInitialEnemyCount = 3;

static const InitialEnemy kInitialEnemies[kInitialEnemyCount] = {
	{ kProfileYellow,  620.0f,  690.0f },
	{ kProfileYellow, 1120.0f,  620.0f },
	{ kProfileBlue,    900.0f,  900.0f }
};

// Three objectives: top left, bottom left and top right
static const int kSpawnerCount = 3;
static const int kSpawnerHp = 10;

static const SpawnerEntity::Def kSpawnerDefs[kSpawnerCount] = {
	{ 227,   9, 26, 52, 267,  27, 11, 29 },	// HADES slab
	{ 227,  91, 26, 66, 267, 109, 11, 44 },	// POSEIDON slab
	{ 227, 183, 26, 38, 267, 201, 11, 17 }	// ZEUS slab
};

static const Vec2f kSpawnerPositions[kSpawnerCount] = {
	{  300.0f, 250.0f },
	{  220.0f, 700.0f },
	{ 1350.0f, 250.0f }
};

// One entry per sprite that has to be drawn with depth sorting
struct DrawItem
{
	float baseY;
	int kind;	// 0 prop, 1 enemy, 2 hero, 3 arrow, 4 spawner
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

// Spiral search for an open tile, used to pick a start point on the endless map
static Vec2f FindWalkableNear(const TileMapEntity& world, int tileX, int tileY, int radius)
{
	for (int r = 0; r <= radius; ++r)
	{
		for (int dy = -r; dy <= r; ++dy)
		{
			for (int dx = -r; dx <= r; ++dx)
			{
				if (std::max(abs(dx), abs(dy)) != r)
				{
					continue;
				}

				const int tx = tileX + dx;
				const int ty = tileY + dy;

				if (!world.IsTileBlocked(tx, ty))
				{
					return { tx * static_cast<float>(TileMapEntity::kTile) + TileMapEntity::kTile * 0.5f,
							 (ty + 1) * static_cast<float>(TileMapEntity::kTile) - 2.0f };
				}
			}
		}
	}

	return { tileX * static_cast<float>(TileMapEntity::kTile) + TileMapEntity::kTile * 0.5f,
			 (tileY + 1) * static_cast<float>(TileMapEntity::kTile) - 2.0f };
}

static int CountActiveEnemies(NinjaEntity enemies[kMaxEnemies])
{
	int count = 0;

	for (int i = 0; i < kMaxEnemies; ++i)
	{
		if (enemies[i].IsActive())
		{
			++count;
		}
	}

	return count;
}

static int SpawnEnemy(NinjaEntity enemies[kMaxEnemies],
	const NinjaProfile* slotProfile[kMaxEnemies],
	const NinjaProfile* type, const Vec2f& feet)
{
	for (int i = 0; i < kMaxEnemies; ++i)
	{
		if (slotProfile[i] == type && !enemies[i].IsActive())
		{
			enemies[i].Init(type, feet);
			return i;
		}
	}

	return -1;
}

// Pick a walkable spot around the objective
static Vec2f SpawnAround(const TileMapEntity& world, const Vec2f& center, int tick)
{
	const float baseAngle = static_cast<float>(tick) * 0.785398f;
	const float radius = 70.0f + (tick % 3) * 18.0f;

	for (int attempt = 0; attempt < 8; ++attempt)
	{
		const float angle = baseAngle + attempt * 0.785398f;
		const Vec2f point{ center.x_ + cosf(angle) * radius,
						   center.y_ + sinf(angle) * radius };

		const int tx = static_cast<int>(point.x_) / TileMapEntity::kTile;
		const int ty = static_cast<int>(point.y_) / TileMapEntity::kTile;

		if (!world.IsTileBlocked(tx, ty))
		{
			return point;
		}
	}

	return center;
}

// ---------------------------------------------------------------------------
// Endless map spawning
// ---------------------------------------------------------------------------

static const int kEndlessMaxEnemies = 7;
static const float kEndlessCullDistance = 2200.0f;

static std::mt19937& Rng()
{
	static std::mt19937 rng(20261031u);
	return rng;
}

// Random walkable tile in a ring around the player, so enemies pop in off screen
static Vec2f RandomSpawnAround(const TileMapEntity& world, const Vec2f& player,
	float minRadius, float maxRadius)
{
	std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> radiusDist(minRadius, maxRadius);

	for (int attempt = 0; attempt < 24; ++attempt)
	{
		const float angle = angleDist(Rng());
		const float radius = radiusDist(Rng());

		const int tx = static_cast<int>(player.x_ + cosf(angle) * radius) / TileMapEntity::kTile;
		const int ty = static_cast<int>(player.y_ + sinf(angle) * radius) / TileMapEntity::kTile;

		if (!world.IsTileBlocked(tx, ty))
		{
			return { tx * static_cast<float>(TileMapEntity::kTile) + TileMapEntity::kTile * 0.5f,
					 (ty + 1) * static_cast<float>(TileMapEntity::kTile) - 2.0f };
		}
	}

	return FindWalkableNear(world, static_cast<int>(player.x_) / TileMapEntity::kTile,
		static_cast<int>(player.y_) / TileMapEntity::kTile, 24);
}

static const NinjaProfile* RandomProfile(const NinjaProfile* profiles[kProfileCount])
{
	std::uniform_int_distribution<int> roll(0, 99);
	const int value = roll(Rng());

	if (value < 55)
	{
		return profiles[kProfileYellow];
	}

	if (value < 85)
	{
		return profiles[kProfileBlue];
	}

	return profiles[kProfileGreen];
}

// The longer the run lasts, the faster the enemies come
static float EndlessSpawnInterval(float elapsed)
{
	const float t = std::min(elapsed, 180.0f);
	return std::max(3.0f, 6.0f - t * 0.015f);
}

static void ResetLevel(TileMapEntity& world, SamuraiEntity& hero,
	NinjaEntity enemies[kMaxEnemies], const NinjaProfile* slotProfile[kMaxEnemies],
	const NinjaProfile* profiles[kProfileCount],
	SpawnerEntity spawners[kSpawnerCount])
{
	for (int i = 0; i < kMaxEnemies; ++i)
	{
		enemies[i].Deactivate();
	}

	if (world.IsInfinite())
	{
		// The endless map has no spawn tile, look for open ground near the origin
		const Vec2f spawn = FindWalkableNear(world, 16, 16, 24);
		hero.SetPosition(spawn);
		hero.Respawn();
	}
	else
	{
		PlaceHeroOnSpawn(hero, world);

		for (int i = 0; i < kInitialEnemyCount; ++i)
		{
			SpawnEnemy(enemies, slotProfile, profiles[kInitialEnemies[i].profile],
				{ kInitialEnemies[i].x, kInitialEnemies[i].y });
		}
	}

	for (int i = 0; i < kSpawnerCount; ++i)
	{
		spawners[i].Init("..\\Resources\\TX Props.png", kSpawnerDefs[i],
			kSpawnerPositions[i], kSpawnerHp);
	}
}

// Loads the chosen stage and resets everything for a fresh run
static void StartLevel(TileMapEntity& world, SamuraiEntity& hero,
	NinjaEntity enemies[kMaxEnemies], const NinjaProfile* slotProfile[kMaxEnemies],
	const NinjaProfile* profiles[kProfileCount],
	SpawnerEntity spawners[kSpawnerCount], int levelIndex)
{
	if (kLevels[levelIndex].infinite)
	{
		world.MakeInfinite(20261029);
	}
	else
	{
		world.LoadMap("..\\Resources\\village.map");
	}

	ResetLevel(world, hero, enemies, slotProfile, profiles, spawners);
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

// Terrain plus every actor, sorted by the line their feet stand on
static void DrawStage(GamesEngineeringBase::Window& canvas, TileMapEntity& world,
	SamuraiEntity& hero, NinjaEntity enemies[kMaxEnemies],
	ArrowEntity arrows[kArrowCount], SpawnerEntity spawners[kSpawnerCount])
{
	world.DrawTerrain(canvas);

	DrawItem drawList[220];
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
	}

	// Enemies and arrows exist on both maps, the objectives only in the village
	for (int i = 0; i < kMaxEnemies; ++i)
	{
		if (!enemies[i].IsActive())
		{
			continue;
		}

		drawList[drawCount].baseY = enemies[i].GetPosition().y_;
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

	if (!world.IsInfinite())
	{
		for (int i = 0; i < kSpawnerCount; ++i)
		{
			if (!spawners[i].IsAlive())
			{
				continue;
			}

			drawList[drawCount].baseY = spawners[i].GetPosition().y_;
			drawList[drawCount].kind = 4;
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
			enemies[item.index].Draw(canvas);
		}
		else if (item.kind == 3)
		{
			arrows[item.index].Draw(canvas);
		}
		else if (item.kind == 4)
		{
			spawners[item.index].Draw(canvas);
		}
		else
		{
			hero.Draw(canvas);
		}
	}
}

static void DrawTextCentered(GamesEngineeringBase::Window& canvas, int y,
	const char* text, int scale, unsigned char r, unsigned char g, unsigned char b)
{
	const int width = static_cast<int>(strlen(text)) * 6 * scale;
	const int x = (static_cast<int>(canvas.getWidth()) - width) / 2;

	// Shadow first, then the text itself
	DrawBitmapText(canvas, x + 2, y + 2, text, scale, 0, 0, 0);
	DrawBitmapText(canvas, x, y, text, scale, r, g, b);
}

// Same, but centred inside an arbitrary horizontal band (used by the cards)
static void DrawTextCenteredIn(GamesEngineeringBase::Window& canvas, int areaX, int areaW,
	int y, const char* text, int scale, unsigned char r, unsigned char g, unsigned char b)
{
	const int width = static_cast<int>(strlen(text)) * 6 * scale;
	const int x = areaX + (areaW - width) / 2;

	DrawBitmapText(canvas, x + 2, y + 2, text, scale, 0, 0, 0);
	DrawBitmapText(canvas, x, y, text, scale, r, g, b);
}

static void FillRect(GamesEngineeringBase::Window& canvas, int x, int y, int w, int h,
	unsigned char r, unsigned char g, unsigned char b)
{
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());

	for (int yy = 0; yy < h; ++yy)
	{
		const int py = y + yy;

		if (py < 0 || py >= winH)
		{
			continue;
		}

		for (int xx = 0; xx < w; ++xx)
		{
			const int px = x + xx;

			if (px < 0 || px >= winW)
			{
				continue;
			}

			canvas.draw(px, py, r, g, b);
		}
	}
}

static void DrawMenu(GamesEngineeringBase::Window& canvas)
{
	// replaced by the five intro screens below
	(void)canvas;
}

// ---------------------------------------------------------------------------
// Intro flow: title -> story -> hero select -> controls -> stage select
// ---------------------------------------------------------------------------

static void DrawUIGradient(GamesEngineeringBase::Window& canvas)
{
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());

	for (int y = 0; y < winH; ++y)
	{
		const int t = y * 255 / winH;
		const unsigned char r = static_cast<unsigned char>(36 - t * 28 / 255);
		const unsigned char g = static_cast<unsigned char>(42 - t * 32 / 255);
		const unsigned char b = static_cast<unsigned char>(64 - t * 50 / 255);

		for (int x = 0; x < winW; ++x)
		{
			canvas.draw(x, y, r, g, b);
		}
	}
}

static void DrawPanel(GamesEngineeringBase::Window& canvas, int x, int y, int w, int h,
	bool highlighted)
{
	FillRect(canvas, x + 4, y + 4, w, h, 8, 8, 12);			// drop shadow
	FillRect(canvas, x, y, w, h, 118, 88, 52);				// wooden frame
	FillRect(canvas, x + 4, y + 4, w - 8, h - 8, 46, 33, 23);	// dark inside
	FillRect(canvas, x + 4, y + 4, w - 8, 3, 150, 114, 68);	// top bevel

	if (highlighted)
	{
		FillRect(canvas, x - 3, y - 3, w + 6, 3, 226, 186, 96);
		FillRect(canvas, x - 3, y + h, w + 6, 3, 226, 186, 96);
		FillRect(canvas, x - 3, y - 3, 3, h + 6, 226, 186, 96);
		FillRect(canvas, x + w, y - 3, 3, h + 6, 226, 186, 96);
	}
}

// Little right pointing triangle used as a selection marker
static void DrawMarker(GamesEngineeringBase::Window& canvas, int x, int y,
	unsigned char r, unsigned char g, unsigned char b)
{
	for (int i = 0; i < 7; ++i)
	{
		for (int j = -i; j <= i; ++j)
		{
			canvas.draw(x + i, y + j, r, g, b);
		}
	}
}

static void DrawTitleScreen(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& button, bool blink)
{
	DrawUIGradient(canvas);

	DrawTextCentered(canvas, 130, "SAMURAI", 10, 255, 210, 90);
	DrawTextCentered(canvas, 246, "THE VILLAGE", 4, 225, 225, 235);

	FillRect(canvas, 282, 316, 460, 3, 210, 170, 90);

	if (button.data != nullptr)
	{
		Renderer_blit_scaled(canvas, button, 322, 2, 90, 36, 422, 372, 2, false);
	}

	if (blink)
	{
		DrawTextCentered(canvas, 500, "PRESS ENTER", 3, 255, 255, 255);
	}

	DrawTextCentered(canvas, 640, "ESC  QUIT", 2, 150, 150, 160);
}

static void DrawIntroScreen(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& scroll, bool blink)
{
	DrawUIGradient(canvas);

	DrawTextCentered(canvas, 60, "THE FALL", 4, 255, 210, 90);

	// Big open scroll from MediavelFull.png, drawn at 3x
	const int scrollW = 175;
	const int scrollH = 150;
	const int dstW = scrollW * 3;
	const int dstH = scrollH * 3;
	const int dstX = (static_cast<int>(canvas.getWidth()) - dstW) / 2;
	const int dstY = 110;

	if (scroll.data != nullptr)
	{
		Renderer_blit_scaled(canvas, scroll, 830, 580, scrollW, scrollH, dstX, dstY, 3, false);
	}

	const char* lines[] = {
		"THE VILLAGE HAS",
		"FALLEN TO THE",
		"EVIL NINJA CLAN.",
		"YOU ARE THE ONLY HERO."
	};

	int y = 226;

	for (int i = 0; i < 4; ++i)
	{
		DrawTextCentered(canvas, y, lines[i], 2, 74, 48, 28);
		y += 28;
	}

	if (blink)
	{
		DrawTextCentered(canvas, 600, "PRESS ENTER", 3, 255, 255, 255);
	}

	DrawTextCentered(canvas, 660, "ESC  BACK", 2, 150, 150, 160);
}

static void DrawCharSelectScreen(GamesEngineeringBase::Window& canvas,
	const GamesEngineeringBase::Image& portrait, int selected, bool blink)
{
	DrawUIGradient(canvas);

	DrawTextCentered(canvas, 52, "CHOOSE YOUR HERO", 4, 255, 210, 90);

	const int cardW = 290;
	const int cardH = 360;
	const int startX = 62;
	const int gap = 14;
	const int cardY = 150;

	for (int i = 0; i < kCharacterCount; ++i)
	{
		const CharacterDef& def = kCharacters[i];
		const int x = startX + i * (cardW + gap);
		const bool highlighted = (i == selected);

		DrawPanel(canvas, x, cardY, cardW, cardH, highlighted);

		if (def.locked)
		{
			FillRect(canvas, x + 4, cardY + 4, cardW - 8, cardH - 8, 20, 18, 20);
		}
		else if (portrait.data != nullptr)
		{
			// The idle frame only has a small character inside it, so crop to it
			const int srcX = 36;
			const int srcY = 47;
			const int srcW = 20;
			const int srcH = 34;
			const int portraitScale = 5;
			const int portraitW = srcW * portraitScale;
			const int portraitH = srcH * portraitScale;

			Renderer_blit_scaled(canvas, portrait, srcX, srcY, srcW, srcH,
				x + (cardW - portraitW) / 2, cardY + 24, portraitScale, false);

			(void)portraitH;
		}

		const int nameY = cardY + 220;
		const int roleY = cardY + 262;

		if (def.locked)
		{
			DrawTextCenteredIn(canvas, x, cardW, nameY, def.name, 3, 110, 100, 100);
			DrawTextCenteredIn(canvas, x, cardW, roleY, "LOCKED", 2, 150, 80, 80);
		}
		else
		{
			DrawTextCenteredIn(canvas, x, cardW, nameY, def.name, 3, 255, 225, 150);
			DrawTextCenteredIn(canvas, x, cardW, roleY, def.role, 2, 200, 200, 210);
			DrawTextCenteredIn(canvas, x, cardW, roleY + 34, "SKILL", 2, 160, 160, 170);
			DrawTextCenteredIn(canvas, x, cardW, roleY + 60, def.skill, 2, 190, 220, 255);
		}

		if (highlighted)
		{
			DrawMarker(canvas, x - 26, cardY + cardH / 2, 226, 186, 96);
		}
	}

	if (blink)
	{
		DrawTextCentered(canvas, 560, "PRESS ENTER TO SELECT", 2, 255, 255, 255);
	}

	DrawTextCentered(canvas, 610, "LEFT / RIGHT  CHANGE", 2, 170, 170, 180);
	DrawTextCentered(canvas, 660, "ESC  BACK", 2, 150, 150, 160);
}

static void DrawControlsScreen(GamesEngineeringBase::Window& canvas, bool blink)
{
	DrawUIGradient(canvas);

	DrawTextCentered(canvas, 60, "SAMURAI", 4, 255, 210, 90);
	DrawTextCentered(canvas, 128, "CONTROLS", 2, 200, 200, 210);

	struct Row
	{
		const char* key;
		const char* action;
	};

	const Row rows[] = {
		{ "WASD", "MOVE" },
		{ "J", "ATTACK" },
		{ "SHIFT", "RUN  (SKILL)" },
		{ "R", "RESTART LEVEL" },
		{ "ESC", "BACK TO MENU" }
	};

	const int rowW = 560;
	const int rowH = 54;
	const int rowX = (static_cast<int>(canvas.getWidth()) - rowW) / 2;
	const int firstY = 190;

	for (int i = 0; i < 5; ++i)
	{
		const int y = firstY + i * (rowH + 12);

		FillRect(canvas, rowX, y, rowW, rowH, 118, 88, 52);
		FillRect(canvas, rowX + 3, y + 3, rowW - 6, rowH - 6, 40, 29, 20);
		FillRect(canvas, rowX + 3, y + 3, rowW - 6, 2, 150, 114, 68);

		DrawBitmapText(canvas, rowX + 26, y + 14, rows[i].key, 2, 255, 225, 150);
		DrawBitmapText(canvas, rowX + 190, y + 14, rows[i].action, 2, 225, 225, 235);
	}

	if (blink)
	{
		DrawTextCentered(canvas, 580, "PRESS ENTER", 3, 255, 255, 255);
	}

	DrawTextCentered(canvas, 650, "ESC  BACK", 2, 150, 150, 160);
}

static void DrawLevelSelectScreen(GamesEngineeringBase::Window& canvas, int selected, bool blink)
{
	DrawUIGradient(canvas);

	DrawTextCentered(canvas, 52, "CHOOSE A STAGE", 4, 255, 210, 90);

	const int cardW = 400;
	const int cardH = 260;
	const int cardY = 200;

	for (int i = 0; i < kLevelCount; ++i)
	{
		const int x = 112 + i * 424;
		const bool highlighted = (i == selected);

		DrawPanel(canvas, x, cardY, cardW, cardH, highlighted);

		DrawTextCenteredIn(canvas, x, cardW, cardY + 60, kLevels[i].name, 4,
			highlighted ? 255 : 200, highlighted ? 225 : 200, highlighted ? 150 : 200);

		DrawTextCenteredIn(canvas, x, cardW, cardY + 130, kLevels[i].desc, 2, 200, 200, 210);

		if (kLevels[i].infinite)
		{
			DrawTextCenteredIn(canvas, x, cardW, cardY + 170, "NO OBJECTIVES", 2, 160, 160, 170);
		}
		else
		{
			DrawTextCenteredIn(canvas, x, cardW, cardY + 170, "DESTROY 3 TOTEMS", 2, 160, 160, 170);
		}

		if (highlighted)
		{
			DrawMarker(canvas, x - 26, cardY + cardH / 2, 226, 186, 96);
		}
	}

	if (blink)
	{
		DrawTextCentered(canvas, 540, "PRESS ENTER TO START", 3, 255, 255, 255);
	}

	DrawTextCentered(canvas, 610, "LEFT / RIGHT  CHANGE", 2, 170, 170, 180);
	DrawTextCentered(canvas, 660, "ESC  BACK", 2, 150, 150, 160);
}

static void DrawResults(GamesEngineeringBase::Window& canvas, bool victory,
	float seconds, int kills)
{
	// Dim whatever is on screen so the panel reads clearly
	const int winW = static_cast<int>(canvas.getWidth());
	const int winH = static_cast<int>(canvas.getHeight());

	for (int y = 0; y < winH; ++y)
	{
		for (int x = 0; x < winW; ++x)
		{
			unsigned char* destination = canvas.backBuffer()
				+ (static_cast<size_t>(y) * winW + x) * 3;

			destination[0] = static_cast<unsigned char>(destination[0] * 90 / 255);
			destination[1] = static_cast<unsigned char>(destination[1] * 90 / 255);
			destination[2] = static_cast<unsigned char>(destination[2] * 90 / 255);
		}
	}

	FillRect(canvas, 272, 200, 480, 320, 16, 18, 28);

	for (int x = 272; x < 752; ++x)
	{
		canvas.draw(x, 200, 210, 170, 90);
		canvas.draw(x, 201, 120, 96, 52);
		canvas.draw(x, 518, 120, 96, 52);
		canvas.draw(x, 519, 210, 170, 90);
	}

	for (int y = 200; y <= 519; ++y)
	{
		canvas.draw(272, y, 210, 170, 90);
		canvas.draw(273, y, 120, 96, 52);
		canvas.draw(750, y, 120, 96, 52);
		canvas.draw(751, y, 210, 170, 90);
	}

	const int minutes = static_cast<int>(seconds) / 60;
	const int secs = static_cast<int>(seconds) % 60;

	char timeText[32];
	std::snprintf(timeText, sizeof(timeText), "TIME %02d:%02d", minutes, secs);

	char killText[32];
	std::snprintf(killText, sizeof(killText), "KILLS %d", kills);

	DrawTextCentered(canvas, 240, victory ? "VICTORY" : "DEFEAT", 5,
		victory ? 255 : 230, victory ? 220 : 90, victory ? 100 : 80);

	DrawTextCentered(canvas, 340, timeText, 3, 235, 235, 235);
	DrawTextCentered(canvas, 390, killText, 3, 235, 235, 235);

	DrawTextCentered(canvas, 460, "PRESS ENTER FOR MENU", 2, 190, 190, 190);
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

	// One animation set per enemy type, shared by every enemy of that type
	NinjaProfile yellowProfile;
	NinjaProfile blueProfile;
	NinjaProfile greenProfile;

	yellowProfile.Load("..\\Resources\\YellowNinja", "yellowNinja", 3, 80.0f, 420.0f, false);
	blueProfile.Load("..\\Resources\\BlueNinja", "BlueNinja", 4, 70.0f, 420.0f, false);
	greenProfile.Load("..\\Resources\\GreenNinja", "GreenNinja", 2, 55.0f, 620.0f, true);

	const NinjaProfile* profiles[kProfileCount] = { &yellowProfile, &blueProfile, &greenProfile };

	// Enemy pool: 8 yellow, 4 blue, 4 green slots, reused by the spawners
	NinjaEntity enemies[kMaxEnemies];
	const NinjaProfile* slotProfile[kMaxEnemies];

	for (int i = 0; i < kMaxEnemies; ++i)
	{
		slotProfile[i] = (i < 8) ? &yellowProfile : ((i < 12) ? &blueProfile : &greenProfile);
	}

	ArrowEntity arrows[kArrowCount];
	int nextArrow = 0;

	for (int i = 0; i < kArrowCount; ++i)
	{
		arrows[i].Init("..\\Resources\\GreenNinja\\GreenNinja - arrow.png");
	}

	SpawnerEntity spawners[kSpawnerCount];

	SamuraiEntity hero({ 0.0f, 0.0f });
	ResetLevel(world, hero, enemies, slotProfile, profiles, spawners);

	// Extra art for the intro screens
	GamesEngineeringBase::Image samuraiIdle;

	if (!samuraiIdle.load("..\\Resources\\samurai\\IDLE.png"))
	{
		std::cerr << "Failed to load samurai IDLE.png" << std::endl;
	}

	float delta_time = timer.dt(); // Get the elapsed time since the last frame.

	bool running = true;
	GameState state = kTitle;

	int charIndex = 0;
	int levelIndex = 0;

	float playTime = 0.0f;
	float endlessTimer = 2.0f;
	int kills = 0;
	int spawnTick = 0;
	float deathTimer = 0.0f;

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
	bool enterDown = false;
	bool navDown = false;

	while (running)
	{
		delta_time = timer.dt();
		++frameCounter;

		const bool enterKey = canvas.keyPressed(VK_RETURN);
		const bool escapeKey = canvas.keyPressed(VK_ESCAPE);

		// ---------------------------------------------------------------
		// Intro flow: title -> story -> hero select -> controls -> stage select
		if (state != kPlaying && state != kVictory && state != kDefeat)
		{
			const bool leftKey = canvas.keyPressed(VK_LEFT) || canvas.keyPressed('A');
			const bool rightKey = canvas.keyPressed(VK_RIGHT) || canvas.keyPressed('D');

			if (state == kTitle)
			{
				if (escapeKey) break;
				if (enterKey && !enterDown) state = kIntro;
			}
			else if (state == kIntro)
			{
				if (escapeKey) state = kTitle;
				if (enterKey && !enterDown) state = kCharSelect;
			}
			else if (state == kCharSelect)
			{
				if (escapeKey) state = kIntro;

				if (leftKey && !navDown) charIndex = (charIndex + kCharacterCount - 1) % kCharacterCount;
				if (rightKey && !navDown) charIndex = (charIndex + 1) % kCharacterCount;

				if (enterKey && !enterDown && !kCharacters[charIndex].locked)
				{
					state = kControls;
				}
			}
			else if (state == kControls)
			{
				if (escapeKey) state = kCharSelect;
				if (enterKey && !enterDown) state = kLevelSelect;
			}
			else if (state == kLevelSelect)
			{
				if (escapeKey) state = kControls;

				if (leftKey && !navDown) levelIndex = (levelIndex + kLevelCount - 1) % kLevelCount;
				if (rightKey && !navDown) levelIndex = (levelIndex + 1) % kLevelCount;

				if (enterKey && !enterDown)
				{
					StartLevel(world, hero, enemies, slotProfile, profiles, spawners, levelIndex);
					playTime = 0.0f;
					endlessTimer = 2.0f;
					kills = 0;
					spawnTick = 0;
					deathTimer = 0.0f;
					state = kPlaying;
				}
			}

			navDown = leftKey || rightKey;
			enterDown = enterKey;

			const bool blink = (frameCounter / 25) % 2 == 0;

			if (state == kTitle)
			{
				DrawTitleScreen(canvas, uiSheet, blink);
			}
			else if (state == kIntro)
			{
				DrawIntroScreen(canvas, uiSheet, blink);
			}
			else if (state == kCharSelect)
			{
				DrawCharSelectScreen(canvas, samuraiIdle, charIndex, blink);
			}
			else if (state == kControls)
			{
				DrawControlsScreen(canvas, blink);
			}
			else
			{
				DrawLevelSelectScreen(canvas, levelIndex, blink);
			}

			canvas.present();
			continue;
		}

		// ---------------------------------------------------------------
		// Results screens: the world stays frozen, the panel is over it
		if (state == kVictory || state == kDefeat)
		{
			if (enterKey && !enterDown)
			{
				state = kTitle;
			}

			if (canvas.keyPressed('R') && !respawnDown)
			{
				StartLevel(world, hero, enemies, slotProfile, profiles, spawners, levelIndex);
				playTime = 0.0f;
					endlessTimer = 2.0f;
				kills = 0;
				spawnTick = 0;
				deathTimer = 0.0f;
				state = kPlaying;
			}

			enterDown = enterKey;
			respawnDown = canvas.keyPressed('R');

			world.FollowCamera(hero.BodyCentre(), canvas);
			game::camera = world.Camera();

			DrawStage(canvas, world, hero, enemies, arrows, spawners);
			DrawResults(canvas, state == kVictory, playTime, kills);
			canvas.present();
			continue;
		}

		// ---------------------------------------------------------------
		// Playing
		if (escapeKey)
		{
			state = kTitle;
			continue;
		}

		playTime += delta_time;

		// 1 = fixed village map, 2 = endless procedural map
		const bool keyOne = canvas.keyPressed('1');
		const bool keyTwo = canvas.keyPressed('2');

		if (keyOne && !keyOneDown)
		{
			if (world.LoadMap("..\\Resources\\village.map"))
			{
				ResetLevel(world, hero, enemies, slotProfile, profiles, spawners);
				playTime = 0.0f;
					endlessTimer = 2.0f;
				kills = 0;
				spawnTick = 0;
				deathTimer = 0.0f;
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

		// Actions: J attack, R restart, H damage / K death are debug helpers
		const bool attackKey = canvas.keyPressed('J');
		const bool hurtKey = canvas.keyPressed('H');
		const bool deathKey = canvas.keyPressed('K');
		const bool respawnKey = canvas.keyPressed('R');

		if (attackKey && !attackDown) { hero.Attack(); }
		if (hurtKey && !hurtDown) { hero.TakeDamage(1); }
		if (deathKey && !deathDown) { hero.Die(); }

		if (respawnKey && !respawnDown)
		{
			ResetLevel(world, hero, enemies, slotProfile, profiles, spawners);
			playTime = 0.0f;
					endlessTimer = 2.0f;
			kills = 0;
			spawnTick = 0;
			deathTimer = 0.0f;
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

		// The surviving objectives are solid as well
		for (int i = 0; i < kSpawnerCount; ++i)
		{
			if (!spawners[i].IsAlive())
			{
				continue;
			}

			float sl = 0.0f, st = 0.0f, sr = 0.0f, sb = 0.0f;
			spawners[i].BodyBox(sl, st, sr, sb);

			hero.BodyBox(left, top, right, bottom);

			if (!(sr < left || sl > right || sb < top || st > bottom))
			{
				hero.SetPosition(previous_feet);
				break;
			}
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

		// Objectives: pulse, spawn enemies while alive, and fall when destroyed
		for (int i = 0; i < kSpawnerCount; ++i)
		{
			spawners[i].Update(delta_time);

			if (world.IsInfinite() || !spawners[i].IsAlive())
			{
				continue;
			}

			if (spawners[i].ReadyToSpawn() && CountActiveEnemies(enemies) < kMaxEnemies - 2)
			{
				const NinjaProfile* type = profiles[spawnTick % kProfileCount];
				++spawnTick;

				SpawnEnemy(enemies, slotProfile, type,
					SpawnAround(world, spawners[i].GetPosition(), spawnTick));
			}
		}

		// Endless map: enemies keep arriving from just outside the view, and the
		// stream gets faster the longer the run lasts
		if (world.IsInfinite())
		{
			endlessTimer -= delta_time;

			if (endlessTimer <= 0.0f)
			{
				endlessTimer = EndlessSpawnInterval(playTime);

				if (CountActiveEnemies(enemies) < kEndlessMaxEnemies)
				{
					const int index = SpawnEnemy(enemies, slotProfile, RandomProfile(profiles),
						RandomSpawnAround(world, hero.GetPosition(), 560.0f, 820.0f));

					// Spawned off screen, so they need a bigger sight range to
					// actually come looking for the player
					if (index >= 0)
					{
						enemies[index].SetAggroRange(4000.0f);
					}
				}
			}

			// Recycle enemies the player managed to outrun
			for (int i = 0; i < kMaxEnemies; ++i)
			{
				if (!enemies[i].IsActive())
				{
					continue;
				}

				const float dx = enemies[i].GetPosition().x_ - hero.GetPosition().x_;
				const float dy = enemies[i].GetPosition().y_ - hero.GetPosition().y_;

				if (dx * dx + dy * dy > kEndlessCullDistance * kEndlessCullDistance)
				{
					enemies[i].Deactivate();
				}
			}
		}

		if (state == kPlaying)
		{
			bool allDestroyed = true;

			for (int i = 0; i < kSpawnerCount; ++i)
			{
				if (spawners[i].IsAlive())
				{
					allDestroyed = false;
				}
			}

			// The endless map has no objectives at all
			if (allDestroyed && !world.IsInfinite())
			{
				state = kVictory;
				std::cerr << "VICTORY\n";
			}
		}

		// Enemies: chase the player, swing or shoot (both maps)
		{
			float heroLeft = 0.0f, heroTop = 0.0f, heroRight = 0.0f, heroBottom = 0.0f;
			hero.BodyBox(heroLeft, heroTop, heroRight, heroBottom);

			for (int i = 0; i < kMaxEnemies; ++i)
			{
				if (!enemies[i].IsActive())
				{
					continue;
				}

				enemies[i].Step(delta_time, hero.GetPosition(), world);

				if (enemies[i].ConsumeShot())
				{
					const Vec2f from{ enemies[i].GetPosition().x_,
									  enemies[i].GetPosition().y_ - 34.0f };

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

				// Archers damage through their arrows only
				if (enemies[i].IsDead() || hero.IsDead() || !enemies[i].IsAttackActive()
					|| enemies[i].IsRanged())
				{
					continue;
				}

				float al = 0.0f, at = 0.0f, ar = 0.0f, ab = 0.0f;
				enemies[i].AttackBox(al, at, ar, ab);

				const bool overlap = !(ar < heroLeft || al > heroRight || ab < heroTop || at > heroBottom);

				if (overlap && enemies[i].TryDamagePlayer())
				{
					hero.TakeDamage(1);
				}
			}

			// Arrows ignore terrain, they only stop on the player or on timeout
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

			// Simple separation so enemies do not stack on each other
			for (int a = 0; a < kMaxEnemies; ++a)
			{
				if (!enemies[a].IsActive() || enemies[a].IsDead())
				{
					continue;
				}

				for (int b = a + 1; b < kMaxEnemies; ++b)
				{
					if (!enemies[b].IsActive() || enemies[b].IsDead())
					{
						continue;
					}

					const Vec2f pa = enemies[a].GetPosition();
					const Vec2f pb = enemies[b].GetPosition();

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

					float bl = 0.0f, bt = 0.0f, br = 0.0f, bb = 0.0f;

					enemies[a].SetPosition({ pa.x_ - nx * push, pa.y_ - ny * push });
					enemies[a].BodyBox(bl, bt, br, bb);

					if (world.IsBoxBlocked(bl, bt, br, bb))
					{
						enemies[a].SetPosition(pa);
					}

					enemies[b].SetPosition({ pb.x_ + nx * push, pb.y_ + ny * push });
					enemies[b].BodyBox(bl, bt, br, bb);

					if (world.IsBoxBlocked(bl, bt, br, bb))
					{
						enemies[b].SetPosition(pb);
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

			for (int i = 0; i < kMaxEnemies; ++i)
			{
				if (enemies[i].TryHit(hero.SwingId(), attackLeft, attackTop,
					attackRight, attackBottom)
					&& enemies[i].IsDead())
				{
					++kills;
				}
			}

			if (!world.IsInfinite())
			{
				for (int i = 0; i < kSpawnerCount; ++i)
				{
					if (spawners[i].TryHit(hero.SwingId(), attackLeft, attackTop,
						attackRight, attackBottom)
						&& !spawners[i].IsAlive())
					{
						++kills;
					}
				}
			}
		}

		// Death of the player ends the run after the death animation
		if (hero.IsDead())
		{
			deathTimer += delta_time;

			if (deathTimer > 1.2f)
			{
				state = kDefeat;
				std::cerr << "DEFEAT\n";
			}
		}

		// Camera follows the middle of the hero body
		world.FollowCamera(hero.BodyCentre(), canvas);
		game::camera = world.Camera();

		// Terrain first, then props and every actor in one depth sorted list
		// (this also freezes the frame behind the results panel)
		DrawStage(canvas, world, hero, enemies, arrows, spawners);

		// Hold B to see the collision box, V for the sword hit box, G for tiles
		if (canvas.keyPressed('B'))
		{
			hero.BodyBox(left, top, right, bottom);
			DrawWorldBox(canvas, left, top, right, bottom, 255, 0, 255);
		}

		if (canvas.keyPressed('V') && attack_active)
		{
			DrawWorldBox(canvas, attackLeft, attackTop, attackRight, attackBottom, 255, 255, 0);
		}

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
