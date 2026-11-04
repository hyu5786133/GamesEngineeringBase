#pragma once

#include "GamesEngineeringBase.h"
#include "PlayerCharacter.h"
#include "TileRenderer.h"
#include "Camera.h"

// ---------------------------------------------------------------------------
// Marisa, the mage ("marisa.png": 64x64 frames, 4 per row).
//
// The sheet stores every animation twice, once per facing direction, so only
// the right facing rows are used here and the renderer mirrors them:
//   idle   rows 3/4   (0 based 2/3)  -> row 3
//   walk   rows 7/8   (0 based 6/7)  -> row 7
//   cast   rows 11/12 (0 based 10/11)-> row 11
//   down   rows 21/22 (0 based 20/21)-> row 21
//
// Attack: slow but heavy magic stars (2 damage, double an arrow).
// Skill (Shift): instantly kills the enemy with the most hit points.
// ---------------------------------------------------------------------------

class MageEntity : public PlayerCharacter {
public:

	enum State { kIdle = 0, kWalk, kCast, kHurt, kDeath };
	enum Anim { kAnimIdle = 0, kAnimWalk, kAnimCast, kAnimHurt, kAnimDeath, kAnimCount };

	static constexpr int kFrame = 64;

	// The art sits around x 32, feet near y 42 inside a 64x64 frame
	static constexpr float kFeetX = 32.0f;
	static constexpr float kFeetY = 42.0f;

	static constexpr float kWalkSpeed = 86.0f;
	static constexpr float kCastCooldown = 1.15f;
	static constexpr float kInvulnerableTime = 1.2f;

	struct AnimDef {
		int row;
		int frames;
		float fps;
		bool loop;
	};

	static constexpr AnimDef kAnims[kAnimCount] = {
		{  3, 2,  5.0f, true  },	// idle
		{  7, 4,  9.0f, true  },	// walk
		{ 11, 2,  8.0f, false },	// cast
		{ 18, 2,  8.0f, false },	// hurt (18 = left facing, 19 = right facing)
		{ 21, 1,  4.0f, false }		// down
	};

	// Hurt pose is held a bit longer than its single frame would last
	static constexpr float kHurtTime = 0.4f;
	static constexpr float kUltCooldown = 20.0f;

	explicit MageEntity(const Vec2f& feet)
		: PlayerCharacter(feet)
	{
		if (!sheet_.load("..\\Resources\\mage\\marisa.png"))
		{
			std::cerr << "[MageEntity] Failed to load marisa.png" << std::endl;
		}
	}

	// -----------------------------------------------------------------------

	void SetInputAxis(const Vec2f& axis) override { inputAxis_ = axis; }

	// Shift is the ultimate: fired once on the rising edge
	void SetSkillHeld(bool held) override
	{
		if (held && !skillHeld_ && ultCooldown_ <= 0.0f)
		{
			ultPending_ = true;
		}

		skillHeld_ = held;
	}

	// Consumes the pending cast and starts the 20 second cooldown
	bool WantsUlt() override
	{
		if (!ultPending_)
		{
			return false;
		}

		ultPending_ = false;
		ultCooldown_ = kUltCooldown;
		return true;
	}

	bool HasUltimate() const override { return true; }
	float UltCooldown() const override { return ultCooldown_; }

	void Attack() override
	{
		if (state_ == kDeath || state_ == kCast || cooldown_ > 0.0f)
		{
			return;
		}

		++swingId_;
		shotFired_ = false;
		shotPending_ = false;
		SetState(kCast);
	}

	bool IsAttackActive() const override { return state_ == kCast; }

	void AttackBox(float& left, float& top, float& right, float& bottom) const override
	{
		left = right = GetPosition().x_;
		top = bottom = GetPosition().y_;
	}

	int SwingId() const override { return swingId_; }

	bool UsesStars() const override { return true; }

	bool WantsArrow(Vec2f& origin, Vec2f& direction) override
	{
		if (!shotPending_)
		{
			return false;
		}

		shotPending_ = false;

		origin = { GetPosition().x_ + facing_ * 20.0f, GetPosition().y_ - 26.0f };
		direction = { static_cast<float>(facing_), 0.0f };
		return true;
	}

	bool TakeDamage(int halfHearts) override
	{
		if (state_ == kDeath || invulnerable_ > 0.0f)
		{
			return false;
		}

		hp_ -= halfHearts;
		invulnerable_ = kInvulnerableTime;

		if (hp_ <= 0)
		{
			hp_ = 0;
			Die();
		}
		else
		{
			SetState(kHurt);
			hurtTime_ = kHurtTime;
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

	void SetHp(int hp) override
	{
		hp_ = hp < 1 ? 1 : (hp > maxHp_ ? maxHp_ : hp);

		if (state_ == kDeath)
		{
			SetState(kIdle);
		}
	}

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

		if (ultCooldown_ > 0.0f)
		{
			ultCooldown_ -= delta_time;

			if (ultCooldown_ < 0.0f)
			{
				ultCooldown_ = 0.0f;
			}
		}

		if (state_ == kDeath)
		{
			return;
		}

		if (state_ == kHurt)
		{
			hurtTime_ -= delta_time;

			if (hurtTime_ <= 0.0f)
			{
				SetState(kIdle);
			}
			return;
		}

		if (state_ == kCast)
		{
			if (!shotFired_ && frame_ >= 1)
			{
				shotFired_ = true;
				shotPending_ = true;
				cooldown_ = kCastCooldown;
			}

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

			Move({ direction.x_ * kWalkSpeed * delta_time,
				   direction.y_ * kWalkSpeed * delta_time });

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

	void Draw(GamesEngineeringBase::Window& window) override
	{
		if (state_ == kDeath && AnimFinished())
		{
			return;
		}

		if (sheet_.data == nullptr)
		{
			return;
		}

		const AnimDef& def = kAnims[CurrentAnim()];

		const int dstX = static_cast<int>(GetPosition().x_ - kFeetX * kScale - game::camera.x_);
		const int dstY = static_cast<int>(GetPosition().y_ - kFeetY * kScale - game::camera.y_);

		int row = def.row;
		bool flip = facing_ < 0;

		if (state_ == kHurt)
		{
			// The hurt art is a mirrored pair: row 18 faces left, row 19 right,
			// so pick the row instead of flipping it
			row = (facing_ > 0) ? 19 : 18;
			flip = false;
		}

		const int srcX = frame_ * kFrame;
		const int srcY = row * kFrame;

		Renderer_blit_scaled(window, sheet_, srcX, srcY, kFrame, kFrame,
			dstX, dstY, kScale, flip);

		// Same white hit flash as the other heroes: their hurt sheets have it
		// baked in, this one is drawn as a white silhouette
		if (state_ == kHurt && ((static_cast<int>(animTime_ * 16.0f) % 2) == 0))
		{
			DrawWhiteSilhouette(window, srcX, srcY, dstX, dstY, flip);
		}
	}

private:

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

	int CurrentAnim() const { return static_cast<int>(state_); }

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

	void DrawWhiteSilhouette(GamesEngineeringBase::Window& window,
		int srcX, int srcY, int dstX, int dstY, bool flip)
	{
		const unsigned char* src = sheet_.data;

		if (src == nullptr)
		{
			return;
		}

		const int imgW = static_cast<int>(sheet_.width);
		const int imgH = static_cast<int>(sheet_.height);
		const int channels = static_cast<int>(sheet_.channels);
		const int winW = static_cast<int>(window.getWidth());
		const int winH = static_cast<int>(window.getHeight());

		for (int y = 0; y < kFrame; ++y)
		{
			for (int x = 0; x < kFrame; ++x)
			{
				const int texelX = srcX + (flip ? (kFrame - 1 - x) : x);
				const int texelY = srcY + y;

				if (texelX < 0 || texelY < 0 || texelX >= imgW || texelY >= imgH)
				{
					continue;
				}

				const unsigned char* p = src + (texelY * imgW + texelX) * channels;

				if (p[3] < 40)
				{
					continue;
				}

				for (int oy = 0; oy < kScale; ++oy)
				{
					for (int ox = 0; ox < kScale; ++ox)
					{
						const int px = dstX + x * kScale + ox;
						const int py = dstY + y * kScale + oy;

						if (px >= 0 && py >= 0 && px < winW && py < winH)
						{
							window.draw(px, py, 255, 255, 255);
						}
					}
				}
			}
		}
	}

	// -----------------------------------------------------------------------

	GamesEngineeringBase::Image sheet_;

	State state_ = kIdle;
	int frame_ = 0;
	int facing_ = 1;
	float animTime_ = 0.0f;
	float cooldown_ = 0.0f;
	float invulnerable_ = 0.0f;
	float hurtTime_ = 0.0f;
	float ultCooldown_ = 0.0f;

	int hp_ = 12;
	int maxHp_ = 12;

	int swingId_ = 0;
	bool shotFired_ = false;
	bool shotPending_ = false;
	bool skillHeld_ = false;
	bool ultPending_ = false;

	Vec2f inputAxis_{ 0.0f, 0.0f };
};
