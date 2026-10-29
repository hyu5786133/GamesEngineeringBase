#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "Camera.h"

// ---------------------------------------------------------------------------
// Stationary training target used to test the melee hit detection before the
// enemy system exists. It is a log pile from the shared spritesheet, has a few
// hit points, flashes when hit and disappears when they run out. Delete it once
// real enemies are in.
// ---------------------------------------------------------------------------

class TrainingDummy : public VisualEntity {
public:

	TrainingDummy(const Vec2f& feet, int hitPoints)
		: VisualEntity(feet)
		, hp_(hitPoints)
		, maxHp_(hitPoints)
	{
	}

	void Update(float delta_time) override
	{
		if (flash_ > 0.0f)
		{
			flash_ -= delta_time;
		}
	}

	void Draw(GamesEngineeringBase::Window& window) override
	{
		if (hp_ <= 0)
		{
			return;
		}

		const int spriteX = static_cast<int>(GetPosition().x_ - 18.0f - game::camera.x_);
		const int spriteY = static_cast<int>(GetPosition().y_ - 49.0f - game::camera.y_);

		const GamesEngineeringBase::Image& sheet = Sheet();

		if (sheet.data != nullptr)
		{
			Renderer_blit(window, sheet, 418, 232, 36, 49, spriteX, spriteY);
		}

		// Remaining hit points as small pips above the pile
		const int pipY = static_cast<int>(GetPosition().y_ - 58.0f - game::camera.y_);
		const int pipX = static_cast<int>(GetPosition().x_ - 10.0f - game::camera.x_);

		for (int i = 0; i < maxHp_; ++i)
		{
			const bool full = i < hp_;

			for (int y = 0; y < 4; ++y)
			{
				for (int x = 0; x < 5; ++x)
				{
					Pixel(window, pipX + i * 7 + x, pipY + y,
						full ? 90 : 45, full ? 220 : 45, full ? 90 : 45);
				}
			}
		}

		// White outline for a short moment after being hit
		if (flash_ > 0.0f)
		{
			for (int y = 0; y <= 49; ++y)
			{
				Pixel(window, spriteX, spriteY + y, 255, 255, 255);
				Pixel(window, spriteX + 35, spriteY + y, 255, 255, 255);
			}
			for (int x = 0; x <= 35; ++x)
			{
				Pixel(window, spriteX + x, spriteY, 255, 255, 255);
				Pixel(window, spriteX + x, spriteY + 49, 255, 255, 255);
			}
		}
	}

	// Hurt box, the lower part of the log pile
	void HurtBox(float& left, float& top, float& right, float& bottom) const
	{
		left = GetPosition().x_ - 16.0f;
		right = GetPosition().x_ + 16.0f;
		bottom = GetPosition().y_;
		top = bottom - 30.0f;
	}

	// Applies one hit if the boxes overlap and this swing has not hit before
	bool TryHit(int swingId, float left, float top, float right, float bottom)
	{
		if (hp_ <= 0 || swingId == lastSwing_)
		{
			return false;
		}

		float hurtLeft = 0.0f, hurtTop = 0.0f, hurtRight = 0.0f, hurtBottom = 0.0f;
		HurtBox(hurtLeft, hurtTop, hurtRight, hurtBottom);

		if (right < hurtLeft || left > hurtRight || bottom < hurtTop || top > hurtBottom)
		{
			return false;
		}

		lastSwing_ = swingId;
		--hp_;
		flash_ = 0.15f;
		return true;
	}

	bool IsDead() const { return hp_ <= 0; }

private:

	static void Pixel(GamesEngineeringBase::Window& window, int x, int y,
		unsigned char r, unsigned char g, unsigned char b)
	{
		if (x < 0 || y < 0 || x >= static_cast<int>(window.getWidth()) || y >= static_cast<int>(window.getHeight()))
		{
			return;
		}
		window.draw(x, y, r, g, b);
	}

	// One shared copy of the spritesheet, loaded on first use
	static GamesEngineeringBase::Image& Sheet()
	{
		static GamesEngineeringBase::Image sheet;
		static bool loaded = false;

		if (!loaded)
		{
			sheet.load("..\\Resources\\assets-spritesheet-standard.png");
			loaded = true;
		}

		return sheet;
	}

	// -----------------------------------------------------------------------

	int hp_ = 1;
	int maxHp_ = 1;
	int lastSwing_ = -1;
	float flash_ = 0.0f;
};
