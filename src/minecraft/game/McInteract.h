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

	void Update(Mc::World &world);
}
