#pragma once

#include "vec2f.h"


class Entity {
public:
    Entity() = default;

    virtual void Update(float delta_time) = 0;
};

// ------------------------------------------------------------

class PositionEntity : public Entity {
public:

    PositionEntity(const Vec2f& position)
        : Entity()
        , position_{ position }
    {

    }

    const Vec2f& GetPosition() const { return position_; }
    void SetPosition(const Vec2f& position) { position_ = position; }
    void Move(const Vec2f& delta)
    {
        position_ = position_ + delta;
    }

private:
    Vec2f position_;

};

class VisualEntity : public PositionEntity
{
public:

    VisualEntity(const Vec2f& position)
        : PositionEntity(position)
    {
    }

    virtual void Update(float delta_time) override = 0;
    virtual void Draw(GamesEngineeringBase::Window& canvas) = 0;
};


class BackgroundEntity : public VisualEntity {

public:
    BackgroundEntity(const Vec2f& position)
        : VisualEntity(position)
    {

    }

    virtual void Update(float delta_time) override {
        // Background might not have any update logic for now
    }

    virtual void Draw(GamesEngineeringBase::Window& canvas) override {
        // Implement background drawing logic here
        for (int y = 0; y < canvas.getHeight(); y += 1)
        {
            for (int x = 0; x < canvas.getWidth(); x += 1)
            {
                canvas.draw(x, y, 95, 200, 255);
            }
        }
    }
};

