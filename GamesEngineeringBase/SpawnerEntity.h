#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "Camera.h"

// ---------------------------------------------------------------------------
// Destroyable objective: a carved stone slab with glowing blue runes, taken
// from TX Props.png. While it is alive it spawns enemies around itself on a
// timer; when its hit points are gone the objective is cleared.
// ---------------------------------------------------------------------------

class SpawnerEntity : public VisualEntity {
public:

	static constexpr int kScale = 2;
	// Slow on purpose: the level is already busy with the initial enemies
	static constexpr float kSpawnInterval = 9.0f;
	static constexpr float kFirstSpawnDelay = 6.0f;

	struct Def
	{
		int slabX, slabY, slabW, slabH;	// stone slab inside the props sheet
		int runeX, runeY, runeW, runeH;	// cyan rune label next to it
	};

	SpawnerEntity()
		: VisualEntity({ 0.0f, 0.0f })
	{
	}

	void Init(const std::string& sheet, const Def& def, const Vec2f& feet, int hp)
	{
		if (!sheet_.load(sheet))
		{
			std::cerr << "[SpawnerEntity] Failed to load: " << sheet << std::endl;
		}

		def_ = def;
		SetPosition(feet);
		hp_ = hp;
		maxHp_ = hp;
		alive_ = hp > 0;
		timer_ = kFirstSpawnDelay;
		glowPhase_ = 0.0f;
		flash_ = 0.0f;
		lastSwing_ = -1;
	}

	void Update(float delta_time)
	{
		glowPhase_ += delta_time;

		if (flash_ > 0.0f)
		{
			flash_ -= delta_time;
		}

		if (alive_ && timer_ > 0.0f)
		{
			timer_ -= delta_time;
		}
	}

	// True once every kSpawnInterval while the objective is alive
	bool ReadyToSpawn()
	{
		if (!alive_ || timer_ > 0.0f)
		{
			return false;
		}

		timer_ = kSpawnInterval;
		return true;
	}

	bool TryHit(int swingId, float left, float top, float right, float bottom)
	{
		if (!alive_ || swingId == lastSwing_)
		{
			return false;
		}

		float hl = 0.0f, ht = 0.0f, hr = 0.0f, hb = 0.0f;
		BodyBox(hl, ht, hr, hb);

		if (right < hl || left > hr || bottom < ht || top > hb)
		{
			return false;
		}

		lastSwing_ = swingId;
		--hp_;
		flash_ = 0.18f;

		if (hp_ <= 0)
		{
			hp_ = 0;
			alive_ = false;
		}

		return true;
	}

	// Solid base, the player cannot walk through the slab
	void BodyBox(float& left, float& top, float& right, float& bottom) const
	{
		const float halfWidth = def_.slabW * kScale * 0.5f;

		left = GetPosition().x_ - halfWidth;
		right = GetPosition().x_ + halfWidth;
		bottom = GetPosition().y_;
		top = bottom - 40.0f;
	}

	bool IsAlive() const { return alive_; }
	int Hp() const { return hp_; }
	int MaxHp() const { return maxHp_; }

	void Draw(GamesEngineeringBase::Window& window) override
	{
		if (!alive_)
		{
			return;
		}

		const int centerX = static_cast<int>(GetPosition().x_ - game::camera.x_);
		const int baseY = static_cast<int>(GetPosition().y_ - game::camera.y_);
		const int slabW = def_.slabW * kScale;
		const int slabH = def_.slabH * kScale;
		const int dstX = centerX - slabW / 2;
		const int dstY = baseY - slabH;

		DrawGlow(window, centerX, baseY - slabH / 2, slabH);

		if (sheet_.data != nullptr)
		{
			Renderer_blit_scaled(window, sheet_,
				def_.slabX, def_.slabY, def_.slabW, def_.slabH,
				dstX, dstY, kScale, false);

			// Cyan runes on the face of the slab, pulsing
			const int pulse = static_cast<int>(140.0f + 80.0f * sinf(glowPhase_ * 3.0f));
			const int runeScale = 2;
			const int runeW = def_.runeW * runeScale;
			const int runeH = def_.runeH * runeScale;

			BlitAlpha(window, sheet_,
				def_.runeX, def_.runeY, def_.runeW, def_.runeH,
				centerX - runeW / 2, dstY + slabH / 4, runeScale, pulse);
		}

		// White flash when hit
		if (flash_ > 0.0f)
		{
			for (int y = 0; y < slabH; ++y)
			{
				for (int x = 0; x < slabW; ++x)
				{
					BlendPixel(window, dstX + x, dstY + y, 255, 255, 255, 70);
				}
			}
		}

		DrawHealthPips(window, centerX, dstY - 12);
	}

private:

	static void BlendPixel(GamesEngineeringBase::Window& window, int x, int y,
		unsigned char r, unsigned char g, unsigned char b, int alpha)
	{
		const int winW = static_cast<int>(window.getWidth());
		const int winH = static_cast<int>(window.getHeight());

		if (x < 0 || y < 0 || x >= winW || y >= winH || alpha <= 0)
		{
			return;
		}

		unsigned char* dst = window.backBuffer()
			+ (static_cast<size_t>(y) * winW + x) * 3;

		const int inverse = 255 - alpha;
		dst[0] = static_cast<unsigned char>((r * alpha + dst[0] * inverse) / 255);
		dst[1] = static_cast<unsigned char>((g * alpha + dst[1] * inverse) / 255);
		dst[2] = static_cast<unsigned char>((b * alpha + dst[2] * inverse) / 255);
	}

	// Soft cyan halo behind the slab
	void DrawGlow(GamesEngineeringBase::Window& window, int centerX, int centerY, int slabH)
	{
		const int radius = slabH / 2 + 18;
		const int pulse = static_cast<int>(18.0f + 12.0f * sinf(glowPhase_ * 3.0f));

		for (int dy = -radius; dy <= radius; ++dy)
		{
			for (int dx = -radius; dx <= radius; ++dx)
			{
				const int d2 = dx * dx + dy * dy;

				if (d2 >= radius * radius)
				{
					continue;
				}

				const int alpha = pulse * (radius * radius - d2) / (radius * radius);

				BlendPixel(window, centerX + dx, centerY + dy, 80, 200, 255, alpha);
			}
		}
	}

	// Nearest neighbour blit with a global alpha multiplier (for the runes)
	static void BlitAlpha(GamesEngineeringBase::Window& window,
		const GamesEngineeringBase::Image& image,
		int srcX, int srcY, int srcW, int srcH,
		int dstX, int dstY, int scale, int alpha)
	{
		const unsigned char* src = image.data;

		if (src == nullptr || alpha <= 0)
		{
			return;
		}

		const int channels = static_cast<int>(image.channels);
		const int imgW = static_cast<int>(image.width);
		const int imgH = static_cast<int>(image.height);
		const int winW = static_cast<int>(window.getWidth());
		const int winH = static_cast<int>(window.getHeight());

		for (int y = 0; y < srcH; ++y)
		{
			const int sy = srcY + y;

			if (sy < 0 || sy >= imgH)
			{
				continue;
			}

			for (int x = 0; x < srcW; ++x)
			{
				const int sx = srcX + x;

				if (sx < 0 || sx >= imgW)
				{
					continue;
				}

				const unsigned char* p = src + (sy * imgW + sx) * channels;

				if (p[3] == 0)
				{
					continue;
				}

				const int a = p[3] * alpha / 255;

				for (int oy = 0; oy < scale; ++oy)
				{
					const int py = dstY + y * scale + oy;

					if (py < 0 || py >= winH)
					{
						continue;
					}

					for (int ox = 0; ox < scale; ++ox)
					{
						const int px = dstX + x * scale + ox;

						if (px >= 0 && px < winW)
						{
							BlendPixel(window, px, py, p[0], p[1], p[2], a);
						}
					}
				}
			}
		}
	}

	void DrawHealthPips(GamesEngineeringBase::Window& window, int centerX, int y)
	{
		const int pipX = centerX - maxHp_ * 3;
		const int pipY = y - 10;

		for (int i = 0; i < maxHp_; ++i)
		{
			const bool full = i < hp_;

			for (int py = 0; py < 4; ++py)
			{
				for (int px = 0; px < 5; ++px)
				{
					BlendPixel(window, pipX + i * 7 + px, pipY + py,
						full ? 120 : 40, full ? 220 : 40, full ? 255 : 40, 255);
				}
			}
		}
	}

	// -----------------------------------------------------------------------

	GamesEngineeringBase::Image sheet_;
	Def def_{};
	Vec2f unused_{ 0.0f, 0.0f };

	bool alive_ = false;
	int hp_ = 1;
	int maxHp_ = 1;
	int lastSwing_ = -1;
	float timer_ = 0.0f;
	float glowPhase_ = 0.0f;
	float flash_ = 0.0f;
};
