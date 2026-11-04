#pragma once

#include "GamesEngineeringBase.h"
#include "Entity.h"
#include "TileRenderer.h"
#include "Camera.h"

// ---------------------------------------------------------------------------
// Mage ultimate effect ("ult magic.png": 12 frames of 32x32, one big star that
// grows and then dissolves). Played once at the position of the victim.
// ---------------------------------------------------------------------------

class UltEffect {
public:

	static constexpr int kFrame = 32;
	static constexpr int kFrames = 12;
	static constexpr int kScale = 6;
	static constexpr float kDuration = 1.1f;

	void Init(const std::string& filename)
	{
		if (!sheet_.load(filename))
		{
			std::cerr << "[UltEffect] Failed to load: " << filename << std::endl;
		}
	}

	void Spawn(const Vec2f& center)
	{
		active_ = true;
		position_ = center;
		time_ = 0.0f;
	}

	void Update(float delta_time)
	{
		if (!active_)
		{
			return;
		}

		time_ += delta_time;

		if (time_ >= kDuration)
		{
			active_ = false;
		}
	}

	void Draw(GamesEngineeringBase::Window& window)
	{
		if (!active_ || sheet_.data == nullptr)
		{
			return;
		}

		int frame = static_cast<int>(time_ / kDuration * kFrames);

		if (frame >= kFrames)
		{
			frame = kFrames - 1;
		}

		const int srcX = (frame % 6) * kFrame;
		const int srcY = (frame / 6) * kFrame;
		const int size = kFrame * kScale;

		Renderer_blit_scaled(window, sheet_, srcX, srcY, kFrame, kFrame,
			static_cast<int>(position_.x_ - game::camera.x_) - size / 2,
			static_cast<int>(position_.y_ - game::camera.y_) - size / 2,
			kScale, false);
	}

	bool IsActive() const { return active_; }

private:

	GamesEngineeringBase::Image sheet_;
	Vec2f position_{ 0.0f, 0.0f };
	bool active_ = false;
	float time_ = 0.0f;
};
