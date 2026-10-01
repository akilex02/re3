#pragma once

#include "McWorld.h"
#include "McRay.h"

namespace McInteract
{
	// A block may only be placed against a real face. A ray that starts inside a block has a zero normal.
	inline bool CanPlace(const Mc::RayHit &hit)
	{
		return hit.hit && !(hit.nx == 0 && hit.ny == 0 && hit.nz == 0);
	}

	// Player body box around a ped origin (origin is about 1.0 above the feet): radius 0.3, feet at cz-1.0, head at cz+0.8.
	// True iff it strictly overlaps the unit cell at (bx,by,bz); touching faces do not count.
	inline bool BodyOverlapsCell(float cx, float cy, float cz, int bx, int by, int bz)
	{
		const float eps = 1e-4f;
		return cx - 0.3f < bx + 1 - eps && cx + 0.3f > bx + eps &&
			cy - 0.3f < by + 1 - eps && cy + 0.3f > by + eps &&
			cz - 1.0f < bz + 1 - eps && cz + 0.8f > bz + eps;
	}

	void Update(Mc::World &world);
}
