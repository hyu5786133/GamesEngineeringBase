#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "MapFile.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Tile based world.
//
// The map is stored as a grid of 32x32 tiles (one byte per tile) plus a small
// list of larger props (houses, trees, bushes) that are drawn on top of the
// tiles. Tilesets are 9-slice style: cells (0,0)..(2,2) hold the corner, edge
// and fill variants of a terrain patch, which lets us pick the right tile from
// the four direct neighbours.
// ---------------------------------------------------------------------------

class TileMapEntity : public VisualEntity {
public:

	static constexpr int kTile = 32;
	static constexpr int kMaxProps = 160;

	enum Terrain : unsigned char {
		kGrass = 0,
		kDirt = 1,
		kStone = 2,
		kWater = 3
	};

	// Source rectangles inside assets-spritesheet-standard.png. The name is what
	// the map file uses, solid is the default block flag (an object line can
	// override it with "solid" or "pass").
	struct SpriteDef {
		const char* name;
		int sx, sy, w, h;
		bool solid;
	};

	static constexpr int kSpriteCount = 18;
	static constexpr SpriteDef kSprites[kSpriteCount] = {
		{ "house_brown", 448,   2, 139, 193, true },
		{ "house_blue",  625,  15, 192, 165, true },
		{ "house_red",   848,  15, 192, 165, true },
		{ "house_green", 848, 207, 192, 165, true },
		{ "tree_pine",   342,   0,  80, 128, true },
		{ "tree_round",  270,   1,  61,  95, true },
		{ "tree_slim",   211,   3,  52,  92, true },
		{ "bush_blue",     9,   8,  62,  51, false },
		{ "bush_red",      9,  68,  62,  51, false },
		{ "bush_leaf",    17, 249,  45,  41, false },
		{ "rock_big",    576, 224,  39,  26, true },
		{ "rock_small",   62, 226,  32,  23, true },
		{ "stump",       289, 152,  40,  28, true },
		{ "sign",        380, 139,  26,  32, false },
		{ "chest",       213, 239,  37,  32, true },
		{ "bench",       266, 235,  49,  39, false },
		{ "logs",        418, 232,  36,  49, true },
		{ "tall_grass",  158, 226,  31,  46, false }
	};

	struct Prop {
		int kind;	// index into kSprites
		int x;		// world pixel position of the top left corner
		int y;
		int baseY;	// world y of the prop's "feet", used for draw sorting
	};

	TileMapEntity()
		: VisualEntity({ 0.0f, 0.0f })
	{
	}

	~TileMapEntity()
	{
		delete[] terrain_;
		delete[] blocked_;
	}

	// -----------------------------------------------------------------------

	void LoadTilesets()
	{
		LoadImage(grass_, "..\\Resources\\tiles-grass medium-32x32.png");
		LoadImage(dirt_, "..\\Resources\\tiles-earth-32x32.png");
		LoadImage(stone_, "..\\Resources\\tiles-stone32x32.png");
		LoadImage(water_, "..\\Resources\\tiles-water.png");
		LoadImage(decor_, "..\\Resources\\tiles-grass-decoration-transparent-32x32.png");
		LoadImage(sprites_, "..\\Resources\\assets-spritesheet-standard.png");
	}

	// Loads a map file (format documented in MapFile.h) and builds the world
	bool LoadMap(const std::string& filename)
	{
		LoadTilesets();

		MapData map;
		std::string error;

		if (!LoadMapFile(filename, map, error))
		{
			std::cerr << "[TileMapEntity] " << error << std::endl;
			return false;
		}

		infinite_ = false;
		width_ = map.width;
		height_ = map.height;
		spawnX_ = map.spawnX;
		spawnY_ = map.spawnY;

		delete[] terrain_;
		delete[] blocked_;

		terrain_ = new unsigned char[width_ * height_];
		blocked_ = new unsigned char[width_ * height_];

		for (int y = 0; y < height_; ++y)
		{
			for (int x = 0; x < width_; ++x)
			{
				terrain_[y * width_ + x] = TerrainFromChar(map.terrain[y][x]);
				blocked_[y * width_ + x] = 0;
			}
		}

		propCount_ = 0;

		for (int i = 0; i < map.objectCount; ++i)
		{
			const MapObjectDef& object = map.objects[i];
			const int kind = FindSprite(object.name);

			if (kind < 0)
			{
				std::cerr << "[TileMapEntity] Unknown object '" << object.name << "'" << std::endl;
				continue;
			}

			const bool blocks = object.solid < 0 ? kSprites[kind].solid : (object.solid != 0);
			AddProp(kind, object.x, object.y, blocks);
		}

		SortProps();
		return true;
	}

	// Switches to the endless procedural map: no file, no fixed bounds
	void MakeInfinite(int seed)
	{
		LoadTilesets();

		delete[] terrain_;
		delete[] blocked_;
		terrain_ = nullptr;
		blocked_ = nullptr;

		infinite_ = true;
		seed_ = seed;
		width_ = 0;
		height_ = 0;
		propCount_ = 0;
		spawnX_ = -1;
		spawnY_ = -1;
		camera_ = { 0.0f, 0.0f };
	}

	bool IsInfinite() const { return infinite_; }

	// -----------------------------------------------------------------------

	void Update(float delta_time) override
	{
		// The map itself has no per frame logic yet
	}

	void Draw(GamesEngineeringBase::Window& canvas) override
	{
		if (!infinite_ && (width_ <= 0 || height_ <= 0))
		{
			return;
		}

		const int camX = static_cast<int>(camera_.x_);
		const int camY = static_cast<int>(camera_.y_);

		const int viewW = static_cast<int>(canvas.getWidth());
		const int viewH = static_cast<int>(canvas.getHeight());

		const int firstX = FloorDiv(camX, kTile);
		const int firstY = FloorDiv(camY, kTile);
		const int cols = viewW / kTile + 2;
		const int rows = viewH / kTile + 2;

		for (int ty = firstY; ty < firstY + rows; ++ty)
		{
			for (int tx = firstX; tx < firstX + cols; ++tx)
			{
				DrawTile(canvas, tx, ty, tx * kTile - camX, ty * kTile - camY);
			}
		}

		if (infinite_)
		{
			DrawScatteredProps(canvas, firstX, firstY, cols, rows, camX, camY);
			return;
		}

		// Props are sorted by their base line so lower objects are drawn in front
		for (int i = 0; i < propCount_; ++i)
		{
			const Prop& p = props_[i];

			if (p.x + kSprites[p.kind].w < camX || p.x > camX + viewW)
			{
				continue;
			}
			if (p.y + kSprites[p.kind].h < camY || p.y > camY + viewH)
			{
				continue;
			}

			DrawProp(canvas, p, p.x - camX, p.y - camY);
		}
	}

	// -----------------------------------------------------------------------

	void FollowCamera(const Vec2f& target, GamesEngineeringBase::Window& canvas)
	{
		const float viewW = static_cast<float>(canvas.getWidth());
		const float viewH = static_cast<float>(canvas.getHeight());

		const float x = target.x_ - viewW * 0.5f;
		const float y = target.y_ - viewH * 0.5f;

		// The endless map has no edges to clamp against
		if (infinite_)
		{
			camera_ = { x, y };
			return;
		}

		const float maxX = static_cast<float>(width_ * kTile) - viewW;
		const float maxY = static_cast<float>(height_ * kTile) - viewH;

		camera_ = { std::clamp(x, 0.0f, maxX), std::clamp(y, 0.0f, maxY) };
	}

	Vec2f Camera() const { return camera_; }

	// Tile the player starts on, falls back to the map centre when not set
	void SpawnTile(int& tx, int& ty) const
	{
		tx = spawnX_ >= 0 ? spawnX_ : width_ / 2;
		ty = spawnY_ >= 0 ? spawnY_ : height_ / 2;
	}

	int Width() const { return width_; }
	int Height() const { return height_; }

	int TerrainAt(int tx, int ty) const
	{
		if (infinite_)
		{
			return InfiniteTerrain(tx, ty);
		}

		if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_)
		{
			return kGrass;
		}
		return terrain_[ty * width_ + tx];
	}

	bool IsTileBlocked(int tx, int ty) const
	{
		if (infinite_)
		{
			return TerrainAt(tx, ty) == kWater;
		}

		if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_)
		{
			return true;
		}

		// Water is always impassable, so is anything a prop marked as solid
		if (terrain_[ty * width_ + tx] == kWater)
		{
			return true;
		}

		return blocked_[ty * width_ + tx] != 0;
	}

	bool IsBlocked(int worldX, int worldY) const
	{
		return IsTileBlocked(FloorDiv(worldX, kTile), FloorDiv(worldY, kTile));
	}

	// True when any tile touched by the axis aligned box is blocked
	bool IsBoxBlocked(float left, float top, float right, float bottom) const
	{
		const int tx0 = FloorDiv(static_cast<int>(left), kTile);
		const int ty0 = FloorDiv(static_cast<int>(top), kTile);
		const int tx1 = FloorDiv(static_cast<int>(right - 1.0f), kTile);
		const int ty1 = FloorDiv(static_cast<int>(bottom - 1.0f), kTile);

		for (int ty = ty0; ty <= ty1; ++ty)
		{
			for (int tx = tx0; tx <= tx1; ++tx)
			{
				if (IsTileBlocked(tx, ty))
				{
					return true;
				}
			}
		}

		return false;
	}

	// -----------------------------------------------------------------------

private:

	static void LoadImage(GamesEngineeringBase::Image& image, const char* filename)
	{
		if (!image.load(filename))
		{
			std::cerr << "[TileMapEntity] Failed to load: " << filename << std::endl;
		}
	}

	// Maps a terrain character from the map file to a tile type
	static unsigned char TerrainFromChar(char tile)
	{
		switch (tile)
		{
		case ',': return kDirt;
		case '#': return kStone;
		case '~': return kWater;
		default:  return kGrass;
		}
	}

	static int FindSprite(const char* name)
	{
		for (int i = 0; i < kSpriteCount; ++i)
		{
			if (std::strcmp(kSprites[i].name, name) == 0)
			{
				return i;
			}
		}
		return -1;
	}

	// Division that rounds towards negative infinity, needed because the
	// endless map has negative tile coordinates
	static int FloorDiv(int value, int divisor)
	{
		if (value >= 0)
		{
			return value / divisor;
		}
		return -((-value + divisor - 1) / divisor);
	}

	// Stable hash of a tile position, used for the endless map
	static unsigned int Hash01(int x, int y, int seed)
	{
		unsigned int h = static_cast<unsigned int>(x) * 374761393u
			+ static_cast<unsigned int>(y) * 668265263u
			+ static_cast<unsigned int>(seed) * 1442695041u;

		h = (h ^ (h >> 13)) * 1274126177u;
		h ^= h >> 16;
		return h & 0xFFFFFFu;
	}

	// Smooth value noise on the integer lattice, returns 0..1
	float ValueNoise(float x, float y) const
	{
		const int x0 = static_cast<int>(floorf(x));
		const int y0 = static_cast<int>(floorf(y));
		const float fx = x - static_cast<float>(x0);
		const float fy = y - static_cast<float>(y0);

		const float v00 = static_cast<float>(Hash01(x0, y0, seed_)) / 16777215.0f;
		const float v10 = static_cast<float>(Hash01(x0 + 1, y0, seed_)) / 16777215.0f;
		const float v01 = static_cast<float>(Hash01(x0, y0 + 1, seed_)) / 16777215.0f;
		const float v11 = static_cast<float>(Hash01(x0 + 1, y0 + 1, seed_)) / 16777215.0f;

		const float sx = fx * fx * (3.0f - 2.0f * fx);
		const float sy = fy * fy * (3.0f - 2.0f * fy);

		const float a = v00 + (v10 - v00) * sx;
		const float b = v01 + (v11 - v01) * sx;
		return a + (b - a) * sy;
	}

	// Two octaves of noise: lakes with a dirt shore, grass everywhere else
	int InfiniteTerrain(int tx, int ty) const
	{
		const float n = 0.65f * ValueNoise(tx * 0.09f, ty * 0.09f)
			+ 0.35f * ValueNoise(tx * 0.27f, ty * 0.27f);

		if (n < 0.33f)
		{
			return kWater;
		}
		if (n < 0.385f)
		{
			return kDirt;
		}
		return kGrass;
	}

	unsigned int PatchHash(int tx, int ty) const
	{
		unsigned int h = static_cast<unsigned int>(tx) * 73856093u
			^ static_cast<unsigned int>(ty) * 19349663u
			^ static_cast<unsigned int>(seed_) * 83492791u;

		h ^= h >> 13;
		h *= 1274126177u;
		return h;
	}

	// The endless map has no object list, so props are scattered by tile hash
	void DrawScatteredProps(GamesEngineeringBase::Window& canvas,
		int firstX, int firstY, int cols, int rows, int camX, int camY)
	{
		for (int ty = firstY; ty < firstY + rows; ++ty)
		{
			for (int tx = firstX; tx < firstX + cols; ++tx)
			{
				if (TerrainAt(tx, ty) != kGrass)
				{
					continue;
				}

				const int roll = static_cast<int>(PatchHash(tx, ty) % 1000u);

				if (roll >= 55)
				{
					continue;
				}

				int kind = 4;

				if (roll < 20)
				{
					kind = 4;	// pine
				}
				else if (roll < 34)
				{
					kind = 5;	// round tree
				}
				else if (roll < 44)
				{
					kind = 7;	// bush
				}
				else
				{
					kind = 11;	// rock
				}

				const SpriteDef& s = kSprites[kind];
				const int baseY = (ty + 1) * kTile;

				Renderer_blit(canvas, sprites_, s.sx, s.sy, s.w, s.h,
					tx * kTile - camX, baseY - s.h - camY);
			}
		}
	}

	void Block(int x0, int y0, int x1, int y1)
	{
		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				if (x >= 0 && y >= 0 && x < width_ && y < height_)
				{
					blocked_[y * width_ + x] = 1;
				}
			}
		}
	}

	// Places a prop so that its bottom edge sits on the given tile row
	void AddProp(int kind, int tileX, int baseTileY, bool blocks)
	{
		if (propCount_ >= kMaxProps)
		{
			return;
		}

		const SpriteDef& s = kSprites[kind];

		Prop& p = props_[propCount_++];
		p.kind = kind;
		p.x = tileX * kTile;
		p.baseY = baseTileY * kTile;
		p.y = p.baseY - s.h;

		if (!blocks)
		{
			return;
		}

		const int tx0 = p.x / kTile;
		const int tx1 = (p.x + s.w - 1) / kTile;
		const int ty0 = p.y / kTile;
		const int ty1 = (p.y + s.h - 1) / kTile;

		if (kind <= 3)
		{
			// Houses block their whole footprint
			Block(tx0, ty0, tx1, ty1);
		}
		else
		{
			// Trees only block the tile their trunk stands on
			Block((p.x + s.w / 2) / kTile, (p.baseY - 1) / kTile,
				  (p.x + s.w / 2) / kTile, (p.baseY - 1) / kTile);
		}
	}

	void SortProps()
	{
		// Insertion sort on the base line, small arrays so this is fine
		for (int i = 1; i < propCount_; ++i)
		{
			const Prop key = props_[i];
			int j = i - 1;

			while (j >= 0 && props_[j].baseY > key.baseY)
			{
				props_[j + 1] = props_[j];
				--j;
			}
			props_[j + 1] = key;
		}
	}

	// Picks the 9-slice cell for a dirt tile from its neighbours
	void DirtCell(int tx, int ty, int& col, int& row) const
	{
		const bool n = TerrainAt(tx, ty - 1) != kGrass;
		const bool s = TerrainAt(tx, ty + 1) != kGrass;
		const bool w = TerrainAt(tx - 1, ty) != kGrass;
		const bool e = TerrainAt(tx + 1, ty) != kGrass;

		row = n ? (s ? 1 : 2) : 0;
		col = w ? (e ? 1 : 2) : 0;
	}

	// Stable pseudo random value for a tile, used to scatter decorations
	unsigned int TileHash(int tx, int ty) const
	{
		return (static_cast<unsigned int>(tx) * 73856093u)
			^ (static_cast<unsigned int>(ty) * 19349663u);
	}

	// Small grass tufts and flowers drawn over plain grass tiles
	void DrawGrassDecor(GamesEngineeringBase::Window& canvas, int tx, int ty, int dx, int dy)
	{
		const unsigned int h = TileHash(tx, ty);
		const int roll = static_cast<int>(h % 100u);

		if (roll < 7)
		{
			// Grass tufts live in cells (0..6, 0)
			Renderer_draw_tile(canvas, decor_, static_cast<int>((h >> 8) % 7u), 0, kTile, dx, dy);
		}
		else if (roll < 11)
		{
			// Flowers live in cells (2..10, 1)
			Renderer_draw_tile(canvas, decor_, 2 + static_cast<int>((h >> 12) % 9u), 1, kTile, dx, dy);
		}
	}

	void DrawTile(GamesEngineeringBase::Window& canvas, int tx, int ty, int dx, int dy)
	{
		// Grass is the base layer everywhere
		Renderer_draw_tile(canvas, grass_, 1, 1, kTile, dx, dy);

		const int terrain = TerrainAt(tx, ty);

		if (terrain == kGrass)
		{
			DrawGrassDecor(canvas, tx, ty, dx, dy);
		}
		else if (terrain == kDirt)
		{
			int col = 1, row = 1;
			DirtCell(tx, ty, col, row);
			Renderer_draw_tile(canvas, dirt_, col, row, kTile, dx, dy);
		}
		else if (terrain == kStone)
		{
			// Cobbles are drawn on top of a dirt base, the gaps between the
			// stones are transparent so the dirt shows through
			int col = 1, row = 1;
			DirtCell(tx, ty, col, row);
			Renderer_draw_tile(canvas, dirt_, col, row, kTile, dx, dy);

			Renderer_draw_tile(canvas, stone_, 3 + static_cast<int>(TileHash(tx, ty) % 3u), 2, kTile, dx, dy);
		}
		else if (terrain == kWater)
		{
			// Row 0 is the top bank, row 5 the bottom bank, rows 1-4 are fill.
			// The fill tile is varied so the ripples do not line up in bands.
			int row = 1 + (tx * 3 + ty * 5) % 4;
			int col = (tx * 5 + ty * 3) % 3;

			if (TerrainAt(tx, ty - 1) != kWater)
			{
				row = 0;
				col = 1;
			}
			else if (TerrainAt(tx, ty + 1) != kWater)
			{
				row = 5;
				col = 1;
			}

			Renderer_draw_tile(canvas, water_, col, row, kTile, dx, dy);
		}
	}

	void DrawProp(GamesEngineeringBase::Window& canvas, const Prop& p, int dstX, int dstY)
	{
		const SpriteDef& s = kSprites[p.kind];
		Renderer_blit(canvas, sprites_, s.sx, s.sy, s.w, s.h, dstX, dstY);
	}

	// -----------------------------------------------------------------------

	GamesEngineeringBase::Image grass_;
	GamesEngineeringBase::Image dirt_;
	GamesEngineeringBase::Image stone_;
	GamesEngineeringBase::Image water_;
	GamesEngineeringBase::Image decor_;
	GamesEngineeringBase::Image sprites_;

	unsigned char* terrain_ = nullptr;
	unsigned char* blocked_ = nullptr;

	int width_ = 0;
	int height_ = 0;
	int spawnX_ = -1;
	int spawnY_ = -1;

	bool infinite_ = false;
	int seed_ = 0;

	Prop props_[kMaxProps];
	int propCount_ = 0;

	Vec2f camera_{ 0.0f, 0.0f };
};
