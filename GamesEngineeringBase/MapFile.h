#pragma once

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Text map file loader.
//
// Format (line based, ';' starts a comment):
//
//   size 40 30          ; width and height in tiles, must come first
//   terrain             ; followed by <height> lines of <width> tile characters
//   ....,,,#...
//   objects             ; followed by: <sprite name> <tile x> <base tile row> [solid|pass]
//   house_brown 5 14
//   bench 17 19
//
// Terrain characters:
//   '.' grass    ',' dirt    '#' stone    '~' water
//
// The base tile row is the row the bottom of the sprite stands on, so a house
// whose door should sit on row 14 is written with base row 14.
// ---------------------------------------------------------------------------

struct MapObjectDef
{
	char name[32];
	int x;
	int y;
	int solid;	// -1 = use the sprite default, 0 = walkable, 1 = solid
};

struct MapData
{
	static constexpr int kMaxWidth = 128;
	static constexpr int kMaxHeight = 128;
	static constexpr int kMaxObjects = 256;

	int width = 0;
	int height = 0;
	int spawnX = -1;	// tile the player starts on, -1 = map centre
	int spawnY = -1;
	char terrain[kMaxHeight][kMaxWidth] = {};
	MapObjectDef objects[kMaxObjects] = {};
	int objectCount = 0;
};

inline std::string MapTrim(const std::string& text)
{
	const size_t first = text.find_first_not_of(" \t\r\n");

	if (first == std::string::npos)
	{
		return "";
	}

	const size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

inline bool LoadMapFile(const std::string& path, MapData& map, std::string& error)
{
	std::ifstream file(path);

	if (!file.is_open())
	{
		error = "cannot open map file: " + path;
		return false;
	}

	enum Stage { kHeader, kTerrain, kObjects };
	Stage stage = kHeader;

	std::string line;
	int lineNo = 0;
	int row = 0;

	while (std::getline(file, line))
	{
		++lineNo;

		// Strip comments
		const size_t comment = line.find(';');

		if (comment != std::string::npos)
		{
			line = line.substr(0, comment);
		}

		line = MapTrim(line);

		if (line.empty())
		{
			continue;
		}

		if (stage == kHeader)
		{
			if (line.rfind("version", 0) == 0)
			{
				continue;
			}

			if (line.rfind("size", 0) == 0)
			{
				std::istringstream iss(line);
				std::string key;

				if (!(iss >> key >> map.width >> map.height)
					|| map.width <= 0 || map.height <= 0
					|| map.width > MapData::kMaxWidth || map.height > MapData::kMaxHeight)
				{
					error = "line " + std::to_string(lineNo) + ": bad size";
					return false;
				}
				continue;
			}

			if (line.rfind("spawn", 0) == 0)
			{
				std::istringstream iss(line);
				std::string key;

				if (!(iss >> key >> map.spawnX >> map.spawnY))
				{
					error = "line " + std::to_string(lineNo) + ": expected 'spawn <x> <y>'";
					return false;
				}
				continue;
			}

			if (line == "terrain")
			{
				if (map.width <= 0 || map.height <= 0)
				{
					error = "line " + std::to_string(lineNo) + ": size must come before terrain";
					return false;
				}

				stage = kTerrain;
				continue;
			}

			if (line == "objects")
			{
				stage = kObjects;
				continue;
			}

			error = "line " + std::to_string(lineNo) + ": unknown directive '" + line + "'";
			return false;
		}

		if (stage == kTerrain)
		{
			if (static_cast<int>(line.size()) != map.width)
			{
				error = "line " + std::to_string(lineNo) + ": expected "
					+ std::to_string(map.width) + " tiles, found " + std::to_string(line.size());
				return false;
			}

			for (int x = 0; x < map.width; ++x)
			{
				const char tile = line[x];

				if (tile != '.' && tile != ',' && tile != '#' && tile != '~')
				{
					error = "line " + std::to_string(lineNo) + ", column " + std::to_string(x + 1)
						+ ": unknown tile '" + std::string(1, tile) + "'";
					return false;
				}

				map.terrain[row][x] = tile;
			}

			++row;

			if (row == map.height)
			{
				stage = kObjects;
			}
			continue;
		}

		// Object lines: name x y [solid|pass]
		// The explicit "objects" heading after the terrain block is optional
		if (line == "objects")
		{
			continue;
		}

		std::istringstream iss(line);
		std::string name;
		int x = 0;
		int y = 0;

		if (!(iss >> name >> x >> y))
		{
			error = "line " + std::to_string(lineNo) + ": expected '<object> <x> <y>'";
			return false;
		}

		int solid = -1;
		std::string flag;

		if (iss >> flag)
		{
			if (flag == "solid")
			{
				solid = 1;
			}
			else if (flag == "pass")
			{
				solid = 0;
			}
			else
			{
				error = "line " + std::to_string(lineNo) + ": expected 'solid' or 'pass', found '" + flag + "'";
				return false;
			}
		}

		if (map.objectCount >= MapData::kMaxObjects)
		{
			error = "line " + std::to_string(lineNo) + ": too many objects";
			return false;
		}

		if (name.size() >= sizeof(map.objects[0].name))
		{
			error = "line " + std::to_string(lineNo) + ": object name too long";
			return false;
		}

		MapObjectDef& object = map.objects[map.objectCount++];
		std::snprintf(object.name, sizeof(object.name), "%s", name.c_str());
		object.x = x;
		object.y = y;
		object.solid = solid;
	}

	if (row != map.height)
	{
		error = "expected " + std::to_string(map.height) + " terrain rows, found " + std::to_string(row);
		return false;
	}

	return true;
}
