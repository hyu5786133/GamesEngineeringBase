#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "Camera.h"

// ---------------------------------------------------------------------------
// Arrow fired by the green ninja. The art is a 8 frame strip of 64x64 arrows
// drawn pointing right, so it is rotated at draw time to match its velocity.
// Arrows ignore terrain on purpose (projectiles fly over walls, only entities
// block them).
// ---------------------------------------------------------------------------

class ArrowEntity {
public:

	static constexpr int kFrame = 64;
	static constexpr int kFrames = 8;
	static constexpr float kScale = 2.0f;

	void Init(const std::string& filename)
	{
		if (!sheet_.load(filename))
		{
			std::cerr << "[ArrowEntity] Failed to load: " << filename << std::endl;
		}
	}

	void Spawn(const Vec2f& center, const Vec2f& direction, float speed, int damage, bool fromPlayer = false)
	{
		active_ = true;
		position_ = center;
		velocity_ = { direction.x_ * speed, direction.y_ * speed };
		damage_ = damage;
		fromPlayer_ = fromPlayer;
		life_ = 3.0f;
		animTime_ = 0.0f;
		frame_ = 0;
	}

	void Update(float delta_time)
	{
		if (!active_)
		{
			return;
		}

		position_ = position_ + velocity_ * delta_time;
		life_ -= delta_time;

		if (life_ <= 0.0f)
		{
			active_ = false;
			return;
		}

		animTime_ += delta_time;
		frame_ = static_cast<int>(animTime_ * 12.0f) % kFrames;
	}

	void Draw(GamesEngineeringBase::Window& window)
	{
		if (!active_)
		{
			return;
		}

		const float angle = atan2f(velocity_.y_, velocity_.x_);

		Renderer_blit_rotated(window, sheet_,
			frame_ * kFrame, 0, kFrame, kFrame,
			position_.x_ - game::camera.x_,
			position_.y_ - game::camera.y_,
			angle, kScale);
	}

	bool IsActive() const { return active_; }
	void Deactivate() { active_ = false; }
	int Damage() const { return damage_; }
	bool FromPlayer() const { return fromPlayer_; }
	Vec2f Position() const { return position_; }

	void HurtBox(float& left, float& top, float& right, float& bottom) const
	{
		left = position_.x_ - 7.0f;
		right = position_.x_ + 7.0f;
		top = position_.y_ - 7.0f;
		bottom = position_.y_ + 7.0f;
	}

private:

	GamesEngineeringBase::Image sheet_;
	Vec2f position_{ 0.0f, 0.0f };
	Vec2f velocity_{ 0.0f, 0.0f };
	bool active_ = false;
	float life_ = 0.0f;
	float animTime_ = 0.0f;
	int frame_ = 0;
	int damage_ = 1;
	bool fromPlayer_ = false;
};
