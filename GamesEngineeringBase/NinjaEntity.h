#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "Camera.h"
#include "TileMapEntity.h"

#include <string>

// ---------------------------------------------------------------------------
// Enemy types and entity.
//
// NinjaProfile holds one loaded set of animations plus the tuning values for a
// type (yellow / blue / green). It is loaded once per type at start up and
// shared by every enemy of that type, so spawners can hand out enemies without
// reloading textures.
//
// NinjaEntity is one instance: 128x128 frames, anchored on its feet point
// (60, 96), lower body collision box. Yellow and blue chase and swing; green
// keeps its distance and shoots arrows (see ArrowEntity.h).
// ---------------------------------------------------------------------------

struct NinjaProfile
{
	GamesEngineeringBase::Image sheets[5];	// idle, walk, attack, hurt, death
	int frames[5] = { 1, 1, 1, 1, 1 };

	int attackActiveFrom = 0;
	int attackActiveTo = 0;
	int releaseFrame = 0;

	bool ranged = false;
	int hp = 3;
	float speed = 70.0f;
	float aggro = 420.0f;

	void Load(const std::string& folder, const std::string& prefix,
		int hitPoints, float moveSpeed, float aggroRadius, bool rangedType)
	{
		hp = hitPoints;
		speed = moveSpeed;
		aggro = aggroRadius;
		ranged = rangedType;

		const std::string base = folder + "\\" + prefix;

		LoadImage(sheets[0], base + " - idle.png");
		LoadImage(sheets[1], base + " - walk.png");
		LoadImage(sheets[2], base + " - attack.png");
		LoadImage(sheets[3], base + " - Whitehit.png");
		LoadImage(sheets[4], base + " - Death.png");

		for (int i = 0; i < 5; ++i)
		{
			frames[i] = (sheets[i].width > 0)
				? static_cast<int>(sheets[i].width) / 128
				: 1;
		}

		const int attackFrames = frames[2];
		attackActiveFrom = attackFrames * 3 / 10;
		attackActiveTo = attackFrames * 6 / 10;
		releaseFrame = attackFrames * 55 / 100;
	}

private:

	static void LoadImage(GamesEngineeringBase::Image& image, const std::string& filename)
	{
		if (!image.load(filename))
		{
			std::cerr << "[NinjaProfile] Failed to load: " << filename << std::endl;
		}
	}
};

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

	static constexpr float kStopDistance = 32.0f;
	static constexpr float kAttackRange = 46.0f;
	static constexpr float kAttackCooldown = 1.6f;
	static constexpr float kRangedCooldown = 1.9f;
	static constexpr float kShootRange = 320.0f;

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

	void Init(const NinjaProfile* profile, const Vec2f& feet)
	{
		profile_ = profile;
		active_ = true;
		Reset(feet);
	}

	// Revives the instance at a new position (used by the spawners)
	void Reset(const Vec2f& feet)
	{
		SetPosition(feet);
		hp_ = profile_ ? profile_->hp : 1;
		maxHp_ = hp_;
		aggroRange_ = profile_ ? profile_->aggro : 420.0f;
		lastSwing_ = -1;
		hitThisSwing_ = false;
		shotPending_ = false;
		shotFired_ = false;
		cooldown_ = 0.0f;
		state_ = kIdle;
		frame_ = 0;
		animTime_ = 0.0f;
	}

	void Deactivate() { active_ = false; }
	bool IsActive() const { return active_; }

	// -----------------------------------------------------------------------

	void Update(float delta_time) override
	{
		// Animation and AI are advanced together in Step()
	}

	// AI step: needs the player position and the map for collision
	void Step(float delta_time, const Vec2f& target, const TileMapEntity& world)
	{
		if (!active_ || profile_ == nullptr)
		{
			return;
		}

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
			if (profile_->ranged && !shotFired_ && frame_ >= profile_->releaseFrame)
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

		if (profile_->ranged)
		{
			// Stop and shoot once the player is inside the shooting range,
			// otherwise keep closing the distance
			if (distance <= kShootRange)
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
		Move(direction * (profile_->speed * delta_time));

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
		cooldown_ = (profile_ && profile_->ranged) ? kRangedCooldown : kAttackCooldown;
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

	// True during the frames where the blade is out
	bool IsAttackActive() const
	{
		return active_ && profile_ != nullptr
			&& state_ == kAttack
			&& frame_ >= profile_->attackActiveFrom
			&& frame_ <= profile_->attackActiveTo;
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
		if (!active_ || state_ == kDeath || swingId == lastSwing_)
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
	bool IsRanged() const { return profile_ != nullptr && profile_->ranged; }

	// Endless mode enemies hunt the player even when they spawn off screen
	void SetAggroRange(float range) { aggroRange_ = range; }

	// True once the death animation has played out, the entity can be removed
	bool ShouldDespawn() const { return state_ == kDeath && AnimFinished(); }

	void Draw(GamesEngineeringBase::Window& window) override
	{
		if (!active_ || profile_ == nullptr)
		{
			return;
		}

		if (state_ == kDeath && AnimFinished())
		{
			return;
		}

		const GamesEngineeringBase::Image& sheet = profile_->sheets[CurrentAnim()];

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

	int CurrentFrames() const
	{
		return profile_ ? profile_->frames[CurrentAnim()] : 1;
	}

	bool AnimFinished() const
	{
		const AnimDef& def = kAnims[CurrentAnim()];
		return animTime_ * def.fps >= static_cast<float>(CurrentFrames());
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
		const int frames = CurrentFrames();

		if (def.loop)
		{
			frame_ = frame % frames;
		}
		else
		{
			frame_ = std::min(frame, frames - 1);
		}
	}

	int CurrentAnim() const { return static_cast<int>(state_); }

	void DrawHealthPips(GamesEngineeringBase::Window& window)
	{
		const int pipY = static_cast<int>(GetPosition().y_ - 78.0f - game::camera.y_);
		const int pipX = static_cast<int>(GetPosition().x_ - maxHp_ * 3.5f - game::camera.x_);

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

	// -----------------------------------------------------------------------

	const NinjaProfile* profile_ = nullptr;
	bool active_ = false;

	State state_ = kIdle;
	int frame_ = 0;
	int facing_ = 1;
	float animTime_ = 0.0f;
	float cooldown_ = 0.0f;
	float aggroRange_ = 420.0f;

	bool shotPending_ = false;
	bool shotFired_ = false;

	int hp_ = 3;
	int maxHp_ = 3;
	int lastSwing_ = -1;
	bool hitThisSwing_ = false;
};
