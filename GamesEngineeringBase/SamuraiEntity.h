#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "PlayerCharacter.h"
#include "TileRenderer.h"
#include "Camera.h"

#include <algorithm>

// ---------------------------------------------------------------------------
// Player controlled samurai.
//
// Each animation is one horizontal strip of 96x96 frames. The character itself
// only occupies a small box inside that frame (measured from the image: x 35..57,
// y 47..81 for the idle frames), so drawing is anchored on the feet point. The
// collision / hurt box is the lower body only, see kBodyW / kBodyH below.
//
// States: idle, walk, run (the speed skill), attack, hurt, death.
// ---------------------------------------------------------------------------

class SamuraiEntity : public PlayerCharacter {
public:

	enum State { kIdle = 0, kWalk, kRun, kAttack, kHurt, kDeath };
	enum Anim { kAnimIdle = 0, kAnimWalk, kAnimRun, kAnimAttack, kAnimHurt, kAnimDeath, kAnimCount };

	static constexpr int kFrameW = 96;
	static constexpr int kFrameH = 96;

	// Feet point inside a frame (bottom centre of the character)
	static constexpr float kFeetX = 46.0f;
	static constexpr float kFeetY = 81.0f;

	// Collision / hurt box is inherited from PlayerCharacter (kBodyW / kBodyH)

	struct AnimDef {
		const char* file;
		int frames;
		float fps;
		bool loop;
	};

	static constexpr AnimDef kAnims[kAnimCount] = {
		{ "..\\Resources\\samurai\\IDLE.png",     10,  8.0f, true  },
		{ "..\\Resources\\samurai\\WALK.png",     12, 12.0f, true  },
		{ "..\\Resources\\samurai\\RUN.png",      16, 16.0f, true  },
		{ "..\\Resources\\samurai\\ATTACK 1.png",  7, 14.0f, false },
		{ "..\\Resources\\samurai\\HURT.png",      4, 10.0f, false },
		{ "..\\Resources\\samurai\\DEATH.png",     9, 10.0f, false }
	};

	// position_ is the feet point in world coordinates
	explicit SamuraiEntity(const Vec2f& feet)
		: PlayerCharacter(feet)
	{
		for (int i = 0; i < kAnimCount; ++i)
		{
			if (!sheets_[i].load(kAnims[i].file))
			{
				std::cerr << "[SamuraiEntity] Failed to load: " << kAnims[i].file << std::endl;
			}
		}
	}

	// -----------------------------------------------------------------------

	void SetInputAxis(const Vec2f& axis) override { inputAxis_ = axis; }
	// Shift is the samurai skill: sprint
	void SetSkillHeld(bool held) override { running_ = held; }

	void Attack() override
	{
		if (state_ != kDeath && state_ != kHurt)
		{
			++swingId_;
			SetState(kAttack);
		}
	}

	void Hurt()
	{
		if (state_ != kDeath)
		{
			SetState(kHurt);
		}
	}

	void Die() override { SetState(kDeath); }

	void Respawn() override
	{
		hp_ = maxHp_;
		invulnerable_ = 0.0f;
		SetState(kIdle);
	}

	bool IsDead() const override { return state_ == kDeath; }

	// -----------------------------------------------------------------------

	int Hp() const override { return hp_; }
	int MaxHp() const override { return maxHp_; }
	bool IsInvulnerable() const override { return invulnerable_ > 0.0f; }

	// Returns true when the damage was actually applied
	bool TakeDamage(int amount) override
	{
		if (state_ == kDeath || invulnerable_ > 0.0f)
		{
			return false;
		}

		hp_ -= amount;
		invulnerable_ = 1.2f;

		if (hp_ <= 0)
		{
			hp_ = 0;
			Die();
		}
		else
		{
			Hurt();
		}

		return true;
	}

	// Collision box in world pixels
	void BodyBox(float& left, float& top, float& right, float& bottom) const override
	{
		const float halfWidth = kBodyW * kScale * 0.5f;

		left = GetPosition().x_ - halfWidth;
		right = GetPosition().x_ + halfWidth;
		bottom = GetPosition().y_;
		top = bottom - kBodyH * kScale;
	}

	Vec2f BodyCentre() const
	{
		return { GetPosition().x_, GetPosition().y_ - kBodyH * kScale * 0.5f };
	}

	// Hit box of the sword swing. Only valid while IsAttackActive() is true,
	// which is the last three frames of the attack animation.
	void AttackBox(float& left, float& top, float& right, float& bottom) const override
	{
		const float halfWidth = kBodyW * kScale * 0.5f;
		const float reach = 26.0f * kScale;
		const float height = 27.0f * kScale;

		const float x = GetPosition().x_;
		const float y = GetPosition().y_;

		bottom = y + 2.0f;
		top = bottom - height;

		const float nearEdge = halfWidth - 4.0f;

		if (facing_ > 0)
		{
			left = x + nearEdge;
			right = left + reach;
		}
		else
		{
			right = x - nearEdge;
			left = right - reach;
		}
	}

	bool IsAttackActive() const override
	{
		// 7 frame attack: 0-3 wind up, 4-6 the blade is out
		return state_ == kAttack && frame_ >= 4 && frame_ <= 6;
	}

	int SwingId() const override { return swingId_; }

	// -----------------------------------------------------------------------

	void Update(float delta_time) override
	{
		if (invulnerable_ > 0.0f)
		{
			invulnerable_ -= delta_time;
		}

		if (state_ == kDeath)
		{
			AdvanceFrame(delta_time);
			return;
		}

		if (state_ == kAttack || state_ == kHurt)
		{
			AdvanceFrame(delta_time);

			const AnimDef& def = kAnims[CurrentAnim()];
			if (animTime_ * def.fps >= static_cast<float>(def.frames))
			{
				SetState(kIdle);
			}
			return;
		}

		const bool moving = inputAxis_.x_ != 0.0f || inputAxis_.y_ != 0.0f;

		if (moving)
		{
			Vec2f direction = inputAxis_;
			const float length = sqrtf(direction.x_ * direction.x_ + direction.y_ * direction.y_);

			if (length > 0.0001f)
			{
				direction.x_ /= length;
				direction.y_ /= length;
			}

			const float speed = running_ ? runSpeed_ : walkSpeed_;
			Move(direction * (speed * delta_time));

			if (direction.x_ > 0.0f)
			{
				facing_ = 1;
			}
			else if (direction.x_ < 0.0f)
			{
				facing_ = -1;
			}

			SetState(running_ ? kRun : kWalk);
		}
		else
		{
			SetState(kIdle);
		}

		AdvanceFrame(delta_time);
	}

	void Draw(GamesEngineeringBase::Window& window) override
	{
		const int anim = CurrentAnim();
		const GamesEngineeringBase::Image& sheet = sheets_[anim];

		if (sheet.data == nullptr)
		{
			return;
		}

		const int dstX = static_cast<int>(GetPosition().x_ - kFeetX * kScale - game::camera.x_);
		const int dstY = static_cast<int>(GetPosition().y_ - kFeetY * kScale - game::camera.y_);

		Renderer_blit_scaled(window, sheet,
			frame_ * kFrameW, 0, kFrameW, kFrameH,
			dstX, dstY, kScale, facing_ < 0);
	}

	State GetState() const { return state_; }

private:

	int CurrentAnim() const { return static_cast<int>(state_); }

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

	// -----------------------------------------------------------------------

	GamesEngineeringBase::Image sheets_[kAnimCount];

	State state_ = kIdle;
	int frame_ = 0;
	int facing_ = 1;
	int swingId_ = 0;
	float animTime_ = 0.0f;

	float walkSpeed_ = 96.0f;	// pixels per second
	float runSpeed_ = 180.0f;

	int maxHp_ = 12;	// half hearts: 6 full hearts
	int hp_ = 12;
	float invulnerable_ = 0.0f;

	Vec2f inputAxis_{ 0.0f, 0.0f };
	bool running_ = false;
};
