#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "Camera.h"

// ---------------------------------------------------------------------------
// Marisa's magic star projectile ("magic star.png": 12 frames of 32x32, six per
// row, one full three star burst animation). Flies straight, ignores terrain,
// hits for 2 damage (double an arrow).
// ---------------------------------------------------------------------------

class StarEntity {
public:

	static constexpr int kFrame = 32;
	static constexpr int kFrames = 12;
	static constexpr int kScale = 3;

	void Init(const std::string& filename)
	{
		if (!sheet_.load(filename))
		{
			std::cerr << "[StarEntity] Failed to load: " << filename << std::endl;
		}
	}

	void Spawn(const Vec2f& center, const Vec2f& direction, float speed, int damage)
	{
		active_ = true;
		position_ = center;
		velocity_ = { direction.x_ * speed, direction.y_ * speed };
		damage_ = damage;
		life_ = 3.0f;
		animTime_ = 0.0f;
	}

	void Update(float delta_time)
	{
		if (!active_)
		{
			return;
		}

		position_ = position_ + velocity_ * delta_time;
		life_ -= delta_time;
		animTime_ += delta_time;

		if (life_ <= 0.0f)
		{
			active_ = false;
		}
	}

	void Draw(GamesEngineeringBase::Window& window)
	{
		if (!active_ || sheet_.data == nullptr)
		{
			return;
		}

		const int frame = static_cast<int>(animTime_ * 16.0f) % kFrames;
		const int srcX = (frame % 6) * kFrame;
		const int srcY = (frame / 6) * kFrame;

		const int size = kFrame * kScale;

		Renderer_blit_scaled(window, sheet_, srcX, srcY, kFrame, kFrame,
			static_cast<int>(position_.x_ - game::camera.x_) - size / 2,
			static_cast<int>(position_.y_ - game::camera.y_) - size / 2,
			kScale, false);
	}

	bool IsActive() const { return active_; }
	void Deactivate() { active_ = false; }
	int Damage() const { return damage_; }
	Vec2f Position() const { return position_; }

	void HurtBox(float& left, float& top, float& right, float& bottom) const
	{
		left = position_.x_ - 10.0f;
		right = position_.x_ + 10.0f;
		top = position_.y_ - 10.0f;
		bottom = position_.y_ + 10.0f;
	}

private:

	GamesEngineeringBase::Image sheet_;
	Vec2f position_{ 0.0f, 0.0f };
	Vec2f velocity_{ 0.0f, 0.0f };
	bool active_ = false;
	float life_ = 0.0f;
	float animTime_ = 0.0f;
	int damage_ = 2;
};
