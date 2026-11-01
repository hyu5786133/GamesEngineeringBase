#pragma once

#include <cstdio>

// ---------------------------------------------------------------------------
// Very small binary save file. Everything the run needs to be rebuilt:
// stage, class, player state, objective health and the live enemies.
// ---------------------------------------------------------------------------

static const int kSaveMaxEnemies = 16;
static const int kSaveMaxSpawners = 3;

struct SaveEnemy
{
	int profile;	// 0 = yellow, 1 = blue, 2 = green
	float x;
	float y;
	int hp;
};

struct SaveGameData
{
	int version = 1;

	int levelIndex = 0;
	int charIndex = 0;

	int playerHp = 12;
	float playerX = 0.0f;
	float playerY = 0.0f;

	float playTime = 0.0f;
	int kills = 0;
	int spawnTick = 0;
	float endlessTimer = 0.0f;

	int spawnerCount = 0;
	int spawnerHp[kSaveMaxSpawners] = { 0, 0, 0 };

	int enemyCount = 0;
	SaveEnemy enemies[kSaveMaxEnemies] = {};
};

inline bool SaveGameToFile(const char* path, const SaveGameData& data)
{
	FILE* file = nullptr;

	if (fopen_s(&file, path, "wb") != 0 || file == nullptr)
	{
		return false;
	}

	const size_t written = fwrite(&data, sizeof(SaveGameData), 1, file);
	fclose(file);

	return written == 1;
}

inline bool LoadGameFromFile(const char* path, SaveGameData& data)
{
	FILE* file = nullptr;

	if (fopen_s(&file, path, "rb") != 0 || file == nullptr)
	{
		return false;
	}

	const size_t read = fread(&data, sizeof(SaveGameData), 1, file);
	fclose(file);

	return read == 1 && data.version == 1;
}
