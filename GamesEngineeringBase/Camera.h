#pragma once

#include "vec2f.h"

// ---------------------------------------------------------------------------
// Shared camera state.
//
// Entities store and draw themselves in world coordinates; the draw code
// subtracts this offset to obtain screen coordinates. The tile map sets it once
// per frame after following the player.
// ---------------------------------------------------------------------------

namespace game
{
	inline Vec2f camera{ 0.0f, 0.0f };
}
