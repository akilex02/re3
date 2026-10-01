#pragma once
#include "McWorld.h"

namespace Mc {

struct CollideResult {
	bool moved;
	bool onGround;	// the last push was upward
};

// (px,py,pz) = centre of the box's feet. See the plan for the exact box.
CollideResult PushOutOfBlocks(const World &w, float &px, float &py, float &pz, float halfWidth, float height);

// True iff a solid block lies under the box's footprint (centre (px,py), half width halfWidth, side faces
// touching exactly do not count) with its top in [pz - tolerance, pz + 0.01]. The small upward margin
// absorbs float error when the feet sit a hair inside the top face.
bool IsStandingOnBlocks(const World &w, float px, float py, float pz, float halfWidth, float tolerance);

}
