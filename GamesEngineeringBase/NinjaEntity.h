#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "Camera.h"
#include "TileMapEntity.h"

#include <string>

// ---------------------------------------------------------------------------
// Melee enemy (yellow ninja today, blue/green later).
//
// Same idea as the samurai: 128x128 frames, character anchored on its feet point
// (60, 96 inside a frame), lower body collision box. Behaviour is a simple chase:
// walk straight at the player when inside the aggro radius, stop at contact range
// and swing; contact with the player deals damage on a cooldown.
// ---------------------------------------------------------------------------

class NinjaEntity : public VisualEntity {
public:

	enum State { kIdle = 0, kWalk, kAttack, kHurt, kDeath };
	enum Anim { kAnimIdle = 0, kAnimWalk, kAnimAttack, kAnimHurt, kAnimDeath, kAnimCount };

	static constexpr int kFrame = 128;
	static constexpr int kScale = 2;
	static constexpr float kFeetX = 60.0f;
	static constexpr float kFeetY = 96.0f;

	// Lower body box, same idea as the player
	static constexpr int kBodyW = 20;
	static constexpr int kBodyH = 18;

	static constexpr float kAggroRange = 420.0f;
	// Must be smaller than the combined half widths of the two body boxes,
	// otherwise the ninja stops just outside contact range and never hits
	static constexpr float kStopDistance = 32.0f;

	struct AnimDef {
		int frames;
		float fps;
		bool loop;
	};

	static constexpr AnimDef kAnims[kAnimCount] = {
		{  8,  8.0f, true  },	// idle
		{ 10, 10.0f, true  },	// walk
		{ 20, 16.0f, false },	// attack
		{  4,  8.0f, false },	// hurt (white flash frames)
		{ 14, 10.0f, false }	// death
	};

	NinjaEntity()
		: VisualEntity({ 0.0f, 0.0f })
	{
	}

	void Init(const std::string& folder, const std::string& prefix, int hp, const Vec2f& feet)
	{
		SetPosition(feet);
		hp_ = hp;
		maxHp_ = hp;
		lastSwing_ = -1;
		cooldown_ = 0.0f;
		state_ = kIdle;
		frame_ = 0;
		animTime_ = 0.0f;

		const std::string base = folder + "\\" + prefix;

		Load(sheets_[kAnimIdle], base + " - idle.png");
		Load(sheets_[kAnimWalk], base + " - walk.png");
		Load(sheets_[kAnimAttack], base + " - attack.png");
		Load(sheets_[kAnimHurt], base + " - Whitehit.png");
		Load(sheets_[kAnimDeath], base + " - Death.png");
	}

	void Reset(const Vec2f& feet)
	{
		SetPosition(feet);
		hp_ = maxHp_;
		lastSwing_ = -1;
		cooldown_ = 0.0f;
		SetState(kIdle);
	}

	// -----------------------------------------------------------------------

	void Update(float delta_time) override
	{
		// Animation and AI are advanced together in Step()
	}

	// AI step: needs the player position and the map for collision
	void Step(float delta_time, const Vec2f& target, const TileMapEntity& world)
	{
		AdvanceFrame(delta_time);

		if (cooldown_ > 0.0f)
		{
			cooldown_ -= delta_time;
		}

		if (state_ == kDeath)
		{
			return;
		}

		if (state_ == kHurt || state_ == kAttack)
		{
			if (AnimFinished())
			{
				SetState(kIdle);
			}
			return;
		}

		const Vec2f position = GetPosition();
		const float dx = target.x_ - position.x_;
		const float dy = target.y_ - position.y_;
		const float distance = sqrtf(dx * dx + dy * dy);

		if (distance > kAggroRange)
		{
			SetState(kIdle);
			return;
		}

		if (distance > kStopDistance)
		{
			const float inverse = 1.0f / (distance > 0.001f ? distance : 1.0f);
			const Vec2f direction{ dx * inverse, dy * inverse };

			const Vec2f previous = position;
			Move(direction * (speed_ * delta_time));

			float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
			BodyBox(left, top, right, bottom);

			if (world.IsBoxBlocked(left, top, right, bottom))
			{
				SetPosition(previous);
			}

			if (direction.x_ > 0.05f)
			{
				facing_ = 1;
			}
			else if (direction.x_ < -0.05f)
			{
				facing_ = -1;
			}

			SetState(kWalk);
		}
		else
		{
			SetState(kIdle);
		}
	}

	bool TryContactDamage()
	{
		if (state_ == kDeath || cooldown_ > 0.0f)
		{
			return false;
		}

		cooldown_ = 1.0f;
		SetState(kAttack);
		return true;
	}

	// Applies one sword hit if the boxes overlap and this swing has not hit yet
	bool TryHit(int swingId, float left, float top, float right, float bottom)
	{
		if (state_ == kDeath || swingId == lastSwing_)
		{
			return false;
		}

		float hurtLeft = 0.0f, hurtTop = 0.0f, hurtRight = 0.0f, hurtBottom = 0.0f;
		BodyBox(hurtLeft, hurtTop, hurtRight, hurtBottom);

		if (right < hurtLeft || left > hurtRight || bottom < hurtTop || top > hurtBottom)
		{
			return false;
		}

		lastSwing_ = swingId;
		--hp_;

		if (hp_ <= 0)
		{
			hp_ = 0;
			SetState(kDeath);
		}
		else
		{
			SetState(kHurt);
		}

		return true;
	}

	// -----------------------------------------------------------------------

	void BodyBox(float& left, float& top, float& right, float& bottom) const
	{
		const float halfWidth = kBodyW * kScale * 0.5f;

		left = GetPosition().x_ - halfWidth;
		right = GetPosition().x_ + halfWidth;
		bottom = GetPosition().y_;
		top = bottom - kBodyH * kScale;
	}

	bool IsDead() const { return state_ == kDeath; }

	// True once the death animation has played out, the entity can be removed
	bool ShouldDespawn() const { return state_ == kDeath && AnimFinished(); }

	void Draw(GamesEngineeringBase::Window& window) override
	{
		if (state_ == kDeath && AnimFinished())
		{
			return;
		}

		const GamesEngineeringBase::Image& sheet = sheets_[CurrentAnim()];

		if (sheet.data != nullptr)
		{
			const int dstX = static_cast<int>(GetPosition().x_ - kFeetX * kScale - game::camera.x_);
			const int dstY = static_cast<int>(GetPosition().y_ - kFeetY * kScale - game::camera.y_);

			Renderer_blit_scaled(window, sheet,
				frame_ * kFrame, 0, kFrame, kFrame,
				dstX, dstY, kScale, facing_ < 0);
		}

		if (state_ != kDeath && hp_ < maxHp_)
		{
			DrawHealthPips(window);
		}
	}

private:

	bool AnimFinished() const
	{
		const AnimDef& def = kAnims[CurrentAnim()];
		return animTime_ * def.fps >= static_cast<float>(def.frames);
	}

	void SetState(State state)
	{
		if (state_ == state)
		{
			return;
		}

		state_ = state;
		animTime_ = 0.0f;
		frame_ = 0;
	}

	void AdvanceFrame(float delta_time)
	{
		animTime_ += delta_time;

		const AnimDef& def = kAnims[CurrentAnim()];
		const int frame = static_cast<int>(animTime_ * def.fps);

		if (def.loop)
		{
			frame_ = frame % def.frames;
		}
		else
		{
			frame_ = std::min(frame, def.frames - 1);
		}
	}

	int CurrentAnim() const { return static_cast<int>(state_); }

	void DrawHealthPips(GamesEngineeringBase::Window& window)
	{
		const int pipY = static_cast<int>(GetPosition().y_ - 78.0f - game::camera.y_);
		const int pipX = static_cast<int>(GetPosition().x_ - 10.0f - game::camera.x_);

		for (int i = 0; i < maxHp_; ++i)
		{
			const bool full = i < hp_;

			for (int y = 0; y < 4; ++y)
			{
				for (int x = 0; x < 5; ++x)
				{
					const int px = pipX + i * 7 + x;
					const int py = pipY + y;

					if (px < 0 || py < 0 || px >= static_cast<int>(window.getWidth()) || py >= static_cast<int>(window.getHeight()))
					{
						continue;
					}

					if (full)
					{
						window.draw(px, py, 230, 60, 60);
					}
					else
					{
						window.draw(px, py, 60, 40, 40);
					}
				}
			}
		}
	}

	static void Load(GamesEngineeringBase::Image& image, const std::string& filename)
	{
		if (!image.load(filename))
		{
			std::cerr << "[NinjaEntity] Failed to load: " << filename << std::endl;
		}
	}

	// -----------------------------------------------------------------------

	GamesEngineeringBase::Image sheets_[kAnimCount];

	State state_ = kIdle;
	int frame_ = 0;
	int facing_ = 1;
	float animTime_ = 0.0f;
	float cooldown_ = 0.0f;
	float speed_ = 70.0f;

	int hp_ = 3;
	int maxHp_ = 3;
	int lastSwing_ = -1;
};
