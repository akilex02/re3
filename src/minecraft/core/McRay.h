#pragma once
#include "McWorld.h"

namespace Mc {

struct RayHit {
	bool hit;
	int x, y, z;		// solid cell that was hit
	int nx, ny, nz;		// face normal toward the ray origin; 0,0,0 if the origin is inside the cell
	float t;		// distance along the direction vector
};

RayHit RayCast(const World &w, float ox, float oy, float oz, float dx, float dy, float dz, float maxDist);

}
