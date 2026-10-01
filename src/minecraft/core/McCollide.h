#pragma once
#include "McWorld.h"

namespace Mc {

struct CollideResult {
	bool moved;
	bool onGround;	// the last push was upward
};

// (px,py,pz) = centre of the box's feet. See the plan for the exact box.
CollideResult PushOutOfBlocks(const World &w, float &px, float &py, float &pz, float halfWidth, float height);

}
