#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Shared interface for the playable classes (samurai / archer / later mage).
// The main loop only talks to this, so the player character can be swapped at
// the character select screen.
//
// Hit points are counted in HALF hearts: a full heart is 2, so an effect that
// halves incoming damage can take 1 instead of 2.
// ---------------------------------------------------------------------------

class PlayerCharacter : public VisualEntity {
public:

	static constexpr int kScale = 2;
	static constexpr int kBodyW = 20;
	static constexpr int kBodyH = 18;

	explicit PlayerCharacter(const Vec2f& feet)
		: VisualEntity(feet)
	{
	}

	virtual ~PlayerCharacter() = default;

	virtual void SetInputAxis(const Vec2f& axis) = 0;
	virtual void SetSkillHeld(bool held) = 0;
	virtual void Attack() = 0;

	virtual bool IsAttackActive() const = 0;
	virtual void AttackBox(float& left, float& top, float& right, float& bottom) const = 0;
	virtual int SwingId() const = 0;

	// Ranged classes return true once per shot and fill in the arrow spawn data
	virtual bool WantsArrow(Vec2f& origin, Vec2f& direction) { return false; }

	// halfHearts is the raw damage: 2 = one full heart
	virtual bool TakeDamage(int halfHearts) = 0;
	virtual void Die() = 0;
	virtual void Respawn() = 0;

	virtual int Hp() const = 0;
	virtual void SetHp(int hp) = 0;
	virtual int MaxHp() const = 0;
	virtual bool IsDead() const = 0;
	virtual bool IsInvulnerable() const = 0;

	// True while the class skill bubble is active (archer defence)
	virtual bool IsGuarding() const { return false; }

	virtual void BodyBox(float& left, float& top, float& right, float& bottom) const = 0;

	Vec2f BodyCentre() const
	{
		return { GetPosition().x_, GetPosition().y_ - kBodyH * kScale * 0.5f };
	}
};
