#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "Camera.h"
#include "TileMapEntity.h"

#include <string>

// ---------------------------------------------------------------------------
// Melee enemy (yellow ninja and blue ninja).
//
// Same idea as the samurai: 128x128 frames, character anchored on its feet point
// (60, 96 inside a frame), lower body collision box. Behaviour is a simple chase:
// walk straight at the player when inside the aggro radius, stop at contact range
// and swing; contact with the player deals damage on a cooldown.
// 
// Ranged enemy (green ninja).
// 
// The variant that draws a bow and shoots arrows at the player. Chasing behaviour
// is similar to the melee types, but stop and shoot once the player is within 
// the shooting range. The arrow is a separate entity, see ArrowEntity.h.
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
	static constexpr float kAttackRange = 46.0f;
	static constexpr float kAttackCooldown = 1.6f;
	static constexpr float kRangedCooldown = 1.9f;
	static constexpr float kShootRange = 320.0f;

	// Frame counts differ per ninja type, so they are read from the loaded
	// sheets; only playback speed and looping live here.
	struct AnimDef {
		float fps;
		bool loop;
	};

	static constexpr AnimDef kAnims[kAnimCount] = {
		{  8.0f, true  },	// idle
		{ 10.0f, true  },	// walk
		{ 16.0f, false },	// attack
		{  8.0f, false },	// hurt (white flash frames)
		{ 10.0f, false }	// death
	};

	NinjaEntity()
		: VisualEntity({ 0.0f, 0.0f })
	{
	}

	void Init(const std::string& folder, const std::string& prefix, int hp,
		float speed, float aggroRange, bool ranged, const Vec2f& feet)
	{
		SetPosition(feet);
		hp_ = hp;
		maxHp_ = hp;
		speed_ = speed;
		aggroRange_ = aggroRange;
		ranged_ = ranged;
		lastSwing_ = -1;
		hitThisSwing_ = false;
		shotPending_ = false;
		shotFired_ = false;
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

		// Derive the frame counts from the sheet widths
		for (int i = 0; i < kAnimCount; ++i)
		{
			frames_[i] = (sheets_[i].width > 0)
				? static_cast<int>(sheets_[i].width) / kFrame
				: 1;
		}

		// The blade is out in the middle of the attack animation
		const int attackFrames = frames_[kAnimAttack];
		attackActiveFrom_ = attackFrames * 3 / 10;
		attackActiveTo_ = attackFrames * 6 / 10;

		// Ranged types release the arrow a bit later, around the apex of the draw
		releaseFrame_ = attackFrames * 55 / 100;
	}

	void Reset(const Vec2f& feet)
	{
		SetPosition(feet);
		hp_ = maxHp_;
		lastSwing_ = -1;
		hitThisSwing_ = false;
		shotPending_ = false;
		shotFired_ = false;
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

		if (state_ == kAttack)
		{
			// Ranged types release the arrow part way through the draw
			if (ranged_ && !shotFired_ && frame_ >= releaseFrame_)
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

		const Vec2f position = GetPosition();
		const float dx = target.x_ - position.x_;
		const float dy = target.y_ - position.y_;
		const float distance = sqrtf(dx * dx + dy * dy);

		if (distance > aggroRange_)
		{
			SetState(kIdle);
			return;
		}

		if (dx > 4.0f)
		{
			facing_ = 1;
		}
		else if (dx < -4.0f)
		{
			facing_ = -1;
		}

		if (ranged_)
		{
			// Stop and shoot once the player is inside the shooting range,
			// otherwise keep closing the distance
			if (distance <= shootRange_)
			{
				if (cooldown_ <= 0.0f)
				{
					BeginAttack();
				}
				else
				{
					SetState(kIdle);
				}
				return;
			}

			MoveTowards(dx, dy, distance, world, delta_time);
			return;
		}

		// Melee types: swing instead of walking when close enough
		if (distance <= kAttackRange && cooldown_ <= 0.0f)
		{
			BeginAttack();
			return;
		}

		if (distance > kStopDistance)
		{
			MoveTowards(dx, dy, distance, world, delta_time);
		}
		else
		{
			SetState(kIdle);
		}
	}

	// Moves straight at the target, refusing moves that would enter a wall
	void MoveTowards(float dx, float dy, float distance, const TileMapEntity& world,
		float delta_time)
	{
		const float inverse = 1.0f / (distance > 0.001f ? distance : 1.0f);
		const Vec2f direction{ dx * inverse, dy * inverse };

		const Vec2f previous = GetPosition();
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

	void BeginAttack()
	{
		hitThisSwing_ = false;
		shotFired_ = false;
		cooldown_ = ranged_ ? kRangedCooldown : kAttackCooldown;
		SetState(kAttack);
	}

	// True once per ranged attack, the owner spawns the arrow when it returns true
	bool ConsumeShot()
	{
		if (!shotPending_)
		{
			return false;
		}

		shotPending_ = false;
		return true;
	}

	// True during the frames where the blade is out, the sword hit box is only
	// valid then
	bool IsAttackActive() const
	{
		return state_ == kAttack && frame_ >= attackActiveFrom_ && frame_ <= attackActiveTo_;
	}

	void AttackBox(float& left, float& top, float& right, float& bottom) const
	{
		const float halfWidth = kBodyW * kScale * 0.5f;
		const float reach = 22.0f * kScale;
		const float height = 22.0f * kScale;

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

	// Returns true once per swing, so one slash cannot hit the player twice
	bool TryDamagePlayer()
	{
		if (hitThisSwing_)
		{
			return false;
		}

		hitThisSwing_ = true;
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
	bool IsRanged() const { return ranged_; }

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
		return animTime_ * def.fps >= static_cast<float>(frames_[CurrentAnim()]);
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
			frame_ = frame % frames_[CurrentAnim()];
		}
		else
		{
			frame_ = std::min(frame, frames_[CurrentAnim()] - 1);
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
	int frames_[kAnimCount] = { 1, 1, 1, 1, 1 };

	State state_ = kIdle;
	int frame_ = 0;
	int facing_ = 1;
	float animTime_ = 0.0f;
	float cooldown_ = 0.0f;
	float speed_ = 70.0f;
	float aggroRange_ = 420.0f;

	bool ranged_ = false;
	float shootRange_ = kShootRange;
	int releaseFrame_ = 0;
	bool shotPending_ = false;
	bool shotFired_ = false;

	int attackActiveFrom_ = 0;
	int attackActiveTo_ = 0;

	int hp_ = 3;
	int maxHp_ = 3;
	int lastSwing_ = -1;
	bool hitThisSwing_ = false;
};
