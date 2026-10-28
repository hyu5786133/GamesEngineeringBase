#pragma once

#include <iostream>

#include "vec2f.h"
#include "renderer.h"
#include "shapes.h"
#include "Camera.h"

class SpriteEntity : public VisualEntity {

public:

    SpriteEntity(const Vec2f& position, const std::string& image_filename)
        : VisualEntity(position)
        , image_()
    {
        if (!image_.load(image_filename))
        {
            std::cerr << "[SpriteEntity] Failed to load image: " << image_filename << std::endl;
        }
    }

    virtual void Update(float delta_time) override {
        // ImageEntity might not have any update logic for now
    }

    virtual void Draw(GamesEngineeringBase::Window& window) override {
        // Entities live in world space, subtract the camera to get screen space
        Renderer_draw_image(window,
            { GetPosition().x_ - game::camera.x_, GetPosition().y_ - game::camera.y_ },
            image_);
    }


    virtual const GamesEngineeringBase::Image& GetImage() const { return image_; }
    virtual int GetImageWidth() const { return GetImage().width; }
    virtual int GetImageHeight() const { return GetImage().height; }

protected:
    GamesEngineeringBase::Image image_;

};

class HeroEntity : public SpriteEntity
{
public:
    explicit HeroEntity(const Vec2f& position)
        : SpriteEntity(position, "..\\Resources\\hero_knight.png")
    {

    }

    void SetInputAxis(const Vec2f& input) { inputAxis_ = input; }
    const Vec2f& GetInputAxis() const { return inputAxis_; }

    void Update(float delta_time) override {
        Move(GetInputAxis() * speed_ * delta_time);
    }

    void Draw(GamesEngineeringBase::Window& window) override {
        SpriteEntity::Draw(window);

        Renderer_drawCircle(window,
            static_cast<int>(GetPosition().x_ - game::camera.x_) + GetImageWidth() / 2,
            static_cast<int>(GetPosition().y_ - game::camera.y_) + GetImageHeight() / 2,
            static_cast<int>(collisionCircle_.getRadius()),
            255, 0, 255,
            1);
    }

protected:
    float speed_ = 64.0f; // Example speed value
    // Gadget gadget{25.0f};// Example gadget with size 5.0f
    Vec2f inputAxis_{ 0.0f, 0.0f };
    Circle collisionCircle_{ 64.0f };

};

