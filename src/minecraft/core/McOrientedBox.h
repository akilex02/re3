#pragma once
#include "McWorld.h"

namespace Mc {

// A box with a yaw but no pitch/roll. Local axes: u = (cos yaw, sin yaw), v = (-sin yaw, cos yaw).
struct OrientedBox {
	float x, y;			// horizontal centre
	float zBottom, zTop;
	float halfU, halfV;
	float yaw;
};

struct BoxPush {
	bool moved;
	bool onTop;		// any push was upward (+Z)
	float dx, dy, dz;	// total translation applied
};

// True if the box overlaps any non-air cell by more than 1e-4.
bool BoxOverlapsBlocks(const World &w, const OrientedBox &b);

// Pushes the box out along the axis of minimal penetration, at most 8 iterations.
BoxPush PushBoxOutOfBlocks(const World &w, OrientedBox &b);

}
