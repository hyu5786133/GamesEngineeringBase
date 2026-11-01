#pragma once

#include "GamesEngineeringBase.h"
#include "PlayerCharacter.h"
#include "TileRenderer.h"
#include "Camera.h"
#include "TileMapEntity.h"

// ---------------------------------------------------------------------------
// Archer: ranged hero from "Archer sheet.png" (64x64 frames, five rows:
// idle / shoot / walk / hurt / death).
//
// Attack: fires an arrow (ArrowEntity with the player faction).
// Skill (hold Shift): strong defence. Movement slows down to 65% and every
// incoming hit is halved (2 half hearts become 1), with a bubble effect drawn
// around the body.
// ---------------------------------------------------------------------------

class ArcherEntity : public PlayerCharacter {
public:

	enum State { kIdle = 0, kWalk, kShoot, kHurt, kDeath };
	enum Anim { kAnimIdle = 0, kAnimWalk, kAnimShoot, kAnimHurt, kAnimDeath, kAnimCount };

	static constexpr int kFrame = 64;

	// Feet point inside a 64x64 frame (the art sits in x 10..42, y 21..64)
	static constexpr float kFeetX = 26.0f;
	static constexpr float kFeetY = 63.0f;

	static constexpr float kWalkSpeed = 92.0f;
	static constexpr float kGuardSpeedScale = 0.65f;
	static constexpr float kShootCooldown = 0.55f;
	static constexpr float kInvulnerableTime = 1.2f;
	static constexpr float kGuardDamageScale = 0.5f;

	struct AnimDef {
		int rowY;
		int frames;
		float fps;
		bool loop;
	};

	static constexpr AnimDef kAnims[kAnimCount] = {
		{   0, 5,  6.0f, true  },	// idle
		{ 128, 8, 10.0f, true  },	// walk
		{  64, 9, 13.0f, false },	// shoot
		{ 192, 5,  9.0f, false },	// hurt
		{ 256, 6,  9.0f, false }	// death
	};

	explicit ArcherEntity(const Vec2f& feet)
		: PlayerCharacter(feet)
	{
		if (!sheets_.load("..\\Resources\\archer\\Archer sheet.png"))
		{
			std::cerr << "[ArcherEntity] Failed to load Archer sheet.png" << std::endl;
		}

		if (!bubble_.load("..\\Resources\\bubble.png"))
		{
			std::cerr << "[ArcherEntity] Failed to load bubble.png" << std::endl;
		}
	}

	// -----------------------------------------------------------------------

	void SetInputAxis(const Vec2f& axis) override { inputAxis_ = axis; }

	void SetSkillHeld(bool held) override { guard_ = held; }

	void Attack() override
	{
		if (state_ == kDeath || state_ == kHurt || state_ == kShoot || cooldown_ > 0.0f)
		{
			return;
		}

		++swingId_;
		shotFired_ = false;
		shotPending_ = false;
		SetState(kShoot);
	}

	bool IsAttackActive() const override { return state_ == kShoot; }

	void AttackBox(float& left, float& top, float& right, float& bottom) const override
	{
		// Ranged class, the arrow carries the damage
		left = right = GetPosition().x_;
		top = bottom = GetPosition().y_;
	}

	int SwingId() const override { return swingId_; }

	bool IsGuarding() const override { return guard_ && state_ != kDeath; }

	// Fires once per shot, at the release frame of the draw animation
	bool WantsArrow(Vec2f& origin, Vec2f& direction) override
	{
		if (!shotPending_)
		{
			return false;
		}

		shotPending_ = false;

		origin = { GetPosition().x_ + facing_ * 18.0f, GetPosition().y_ - 30.0f };
		direction = { static_cast<float>(facing_), 0.0f };
		return true;
	}

	bool TakeDamage(int halfHearts) override
	{
		if (state_ == kDeath || invulnerable_ > 0.0f)
		{
			return false;
		}

		int amount = halfHearts;

		if (guard_)
		{
			amount = static_cast<int>(amount * kGuardDamageScale);

			if (amount < 1)
			{
				amount = 1;
			}
		}

		hp_ -= amount;
		invulnerable_ = kInvulnerableTime;

		if (hp_ <= 0)
		{
			hp_ = 0;
			Die();
		}
		else
		{
			SetState(kHurt);
		}

		return true;
	}

	void Die() override { SetState(kDeath); }

	void Respawn() override
	{
		hp_ = maxHp_;
		invulnerable_ = 0.0f;
		cooldown_ = 0.0f;
		SetState(kIdle);
	}

	int Hp() const override { return hp_; }
	int MaxHp() const override { return maxHp_; }
	bool IsDead() const override { return state_ == kDeath; }
	bool IsInvulnerable() const override { return invulnerable_ > 0.0f; }

	void BodyBox(float& left, float& top, float& right, float& bottom) const override
	{
		const float halfWidth = kBodyW * kScale * 0.5f;

		left = GetPosition().x_ - halfWidth;
		right = GetPosition().x_ + halfWidth;
		bottom = GetPosition().y_;
		top = bottom - kBodyH * kScale;
	}

	// -----------------------------------------------------------------------

	void Update(float delta_time) override
	{
		AdvanceFrame(delta_time);

		if (cooldown_ > 0.0f)
		{
			cooldown_ -= delta_time;
		}

		if (invulnerable_ > 0.0f)
		{
			invulnerable_ -= delta_time;
		}

		if (state_ == kDeath)
		{
			return;
		}

		if (state_ == kShoot)
		{
			// Release the arrow in the middle of the draw
			if (!shotFired_ && frame_ >= kReleaseFrame)
			{
				shotFired_ = true;
				shotPending_ = true;
			}

			if (AnimFinished())
			{
				SetState(kIdle);
			}
			return;
		}

		if (state_ == kHurt)
		{
			if (AnimFinished())
			{
				SetState(kIdle);
			}
			return;
		}

		Vec2f direction = inputAxis_;

		if (direction.x_ != 0.0f || direction.y_ != 0.0f)
		{
			const float length = sqrtf(direction.x_ * direction.x_ + direction.y_ * direction.y_);
			direction.x_ /= length;
			direction.y_ /= length;

			const float speed = kWalkSpeed * (guard_ ? kGuardSpeedScale : 1.0f);

			Move({ direction.x_ * speed * delta_time, direction.y_ * speed * delta_time });

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

		bubbleTime_ += delta_time;
	}

	void Draw(GamesEngineeringBase::Window& window) override
	{
		if (state_ == kDeath && AnimFinished())
		{
			return;
		}

		if (IsGuarding())
		{
			DrawBubble(window);
		}

		const AnimDef& def = kAnims[CurrentAnim()];
		const int srcY = def.rowY;

		const int dstX = static_cast<int>(GetPosition().x_ - kFeetX * kScale - game::camera.x_);
		const int dstY = static_cast<int>(GetPosition().y_ - kFeetY * kScale - game::camera.y_);

		Renderer_blit_scaled(window, sheets_,
			frame_ * kFrame, srcY, kFrame, kFrame,
			dstX, dstY, kScale, facing_ < 0);
	}

private:

	static constexpr int kReleaseFrame = 6;

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

	bool AnimFinished() const
	{
		const AnimDef& def = kAnims[CurrentAnim()];
		return animTime_ * def.fps >= static_cast<float>(def.frames);
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

	// bubble.png holds one ring animation in 12 frames (32x32, 6 per row).
	// The shield plays a short ping-pong over the frames where the ring is
	// closed (3..6), so it reads as a single breathing dome on the body.
	void DrawBubble(GamesEngineeringBase::Window& window)
	{
		if (bubble_.data == nullptr)
		{
			return;
		}

		static const int kSequence[6] = { 3, 4, 5, 6, 5, 4 };
		const int step = static_cast<int>(bubbleTime_ * 7.0f) % 6;
		const int frame = kSequence[step];

		const int srcX = (frame % 6) * 32;
		const int srcY = (frame / 6) * 32;

		const int scale = 4;			// 32 * 4 cell, the circle itself is ~92x80
		const int size = 32 * scale;

		const int centerX = static_cast<int>(GetPosition().x_ - game::camera.x_);
		const int centerY = static_cast<int>(GetPosition().y_ - 38.0f - game::camera.y_);

		Renderer_blit_scaled(window, bubble_, srcX, srcY, 32, 32,
			centerX - size / 2, centerY - size / 2, scale, false);
	}

	// -----------------------------------------------------------------------

	GamesEngineeringBase::Image sheets_;
	GamesEngineeringBase::Image bubble_;

	State state_ = kIdle;
	int frame_ = 0;
	int facing_ = 1;
	float animTime_ = 0.0f;
	float bubbleTime_ = 0.0f;
	float cooldown_ = 0.0f;
	float invulnerable_ = 0.0f;

	int hp_ = 12;
	int maxHp_ = 12;

	int swingId_ = 0;
	bool shotFired_ = false;
	bool shotPending_ = false;
	bool guard_ = false;

	Vec2f inputAxis_{ 0.0f, 0.0f };
};
