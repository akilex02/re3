#pragma once

#include <math.h>
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
	// BodyOverlapsCell below hardcodes these same numbers; keep them in sync.
	static const float bodyFeetOffset = 1.0f;
	static const float bodyHalfWidth = 0.3f;
	static const float bodyHeight = 1.8f;

	// True iff it strictly overlaps the unit cell at (bx,by,bz); touching faces do not count.
	inline bool BodyOverlapsCell(float cx, float cy, float cz, int bx, int by, int bz)
	{
		const float eps = 1e-4f;
		return cx - 0.3f < bx + 1 - eps && cx + 0.3f > bx + eps &&
			cy - 0.3f < by + 1 - eps && cy + 0.3f > by + eps &&
			cz - 1.0f < bz + 1 - eps && cz + 0.8f > bz + eps;
	}

	// Distance along the unit ray direction (dx,dy,dz) from the camera to the point nearest the head,
	// clamped at 0. The interaction ray starts there so it never begins behind the player.
	inline float RayStartOffset(float cx, float cy, float cz, float hx, float hy, float hz, float dx, float dy, float dz)
	{
		float t0 = (hx - cx) * dx + (hy - cy) * dy + (hz - cz) * dz;
		return t0 > 0.0f ? t0 : 0.0f;
	}

	// Cell just in front of a GTA surface hit: floor(point + normal * 0.05) per axis.
	// floorf, not an int cast: coordinates may be negative.
	inline void SurfacePlacementCell(float px, float py, float pz, float nx, float ny, float nz, int &cx, int &cy, int &cz)
	{
		cx = (int)floorf(px + nx * 0.05f);
		cy = (int)floorf(py + ny * 0.05f);
		cz = (int)floorf(pz + nz * 0.05f);
	}

	// The line-of-sight test is two-sided, so a back-face hit has a normal pointing away from the viewer.
	// Flip it toward the viewer when it points along the ray direction (dx,dy,dz).
	inline void FaceNormalToward(float &nx, float &ny, float &nz, float dx, float dy, float dz)
	{
		if(nx * dx + ny * dy + nz * dz > 0.0f){
			nx = -nx;
			ny = -ny;
			nz = -nz;
		}
	}

	// True while the player may interact: a living, controllable player outside cutscenes and replays.
	// Shared by Update, the hotbar keys and the hotbar drawing.
	bool CanInteract(void);

	uint8_t GetSelectedBlock(void);
	// Ignores ids outside 1..CLASSIC_BLOCKS.
	void SetSelectedBlock(uint8_t id);

	void Update(Mc::World &world);
}
