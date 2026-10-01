# Peds and vehicles collide with blocks — Implementation Plan (phase 1b)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Non-player peds and cars stop against placed voxel blocks like walls.

**Architecture:** Pure core functions (`GetBounds`, oriented-box push-out, sweep) tested outside the game, plus one game-layer unit `McEntities` called from the existing `McMode::Update` hook after `CWorld::Process()`. No Rockstar files change.

**Tech Stack:** C++11; re3 CMake/Ninja build; the standalone test project in `tests/minecraft`.

**Spec:** `docs/superpowers/specs/2026-09-30-minecraft-entity-collision-design.md` (parent: `2026-09-30-minecraft-mode-design.md`)

## Global Constraints

- C++11 only. Core (`src/minecraft/core/`) uses only the standard library and is unit-tested in `tests/minecraft/` (never put test files under `src/`).
- Game layer (`src/minecraft/game/`) follows `CODING_STYLE.md`: tabs, return type on its own line, brace on the next line for function definitions, no braces around single statements, `int *ptr`, project typedefs (`uint8`, `int32`), `nil`. Everything inside `#ifdef MINECRAFT_MODE`. The include order that compiles for ped/world headers is `Ped.h`, `PlayerPed.h`, then `PlayerInfo.h`, `World.h`.
- No Rockstar file is modified in this plan. Never stage `src/core/config.h` (the user's uncommitted reformatting) or `CLAUDE.md`; stage by explicit path.
- GTA axes: X east, Y north, Z up; block `(x,y,z)` occupies `[x,x+1) x [y,y+1) x [z,z+1)`. Overlap tolerance `EPS = 1e-4` (touching faces do not overlap).
- Ped body box: half width 0.3, height 1.8, origin 1.0 above the feet (`McInteract.h`: `bodyFeetOffset`, `bodyHalfWidth`, `bodyHeight`).
- Run tests: `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j && build/mctest/mctests` (from `/home/akilex/Descargas/gtas/re3`); game build: `cd build && cmake . && cmake --build . -j$(nproc)`. Do not run the game in implementer tasks (the user playtests).
- Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

- Negative coordinates in the oriented-box SAT and the sweep (Task 2, 3).
- A box that starts inside solid blocks must terminate (iteration cap) without moving garbage distances (Task 2).
- A car moving fast (30 m/s = 1 m per frame at 30 fps) must not tunnel through a 1-block wall (Task 3).
- Peds in vehicles, dead peds, the player ped, and entities with `bUsesCollision == false` must be skipped (Task 4, 5).
- With no blocks (empty world) the per-frame cost is one bounds test and nothing else changes (Task 4).

## File Structure

```
src/minecraft/core/McWorld.h/.cpp        + World::GetBounds
src/minecraft/core/McOrientedBox.h/.cpp  OrientedBox, BoxOverlapsBlocks, PushBoxOutOfBlocks, SweepBox
src/minecraft/game/McEntities.h/.cpp     peds + vehicles per frame
src/minecraft/game/McMode.cpp            + McEntities::Update call
tests/minecraft/test_bounds.cpp, test_obox.cpp, test_sweep.cpp
```

---

### Task 1: `World::GetBounds`

**Files:** Modify `src/minecraft/core/McWorld.h`, `src/minecraft/core/McWorld.cpp`; Create `tests/minecraft/test_bounds.cpp`.

**Interfaces:**
- Produces: `bool World::GetBounds(int &minX, int &minY, int &minZ, int &maxX, int &maxY, int &maxZ) const` — false when the world has no chunks; otherwise the inclusive cell bounds of the union of all occupied chunks (chunk `c` spans cells `c*16 .. c*16+15`).

- [ ] **Step 1: Write the failing tests** — `tests/minecraft/test_bounds.cpp`:
```cpp
#include "mctest.h"
#include "McWorld.h"

using namespace Mc;

MC_TEST(bounds_empty_world_has_none)
{
	World w;
	int a, b, c, d, e, f;
	MC_CHECK(!w.GetBounds(a, b, c, d, e, f));
}

MC_TEST(bounds_single_chunk)
{
	World w;
	w.Set(3, 4, 5, BLOCK_STONE);
	int x0, y0, z0, x1, y1, z1;
	MC_CHECK(w.GetBounds(x0, y0, z0, x1, y1, z1));
	MC_CHECK_EQ(x0, 0); MC_CHECK_EQ(y0, 0); MC_CHECK_EQ(z0, 0);
	MC_CHECK_EQ(x1, 15); MC_CHECK_EQ(y1, 15); MC_CHECK_EQ(z1, 15);
}

MC_TEST(bounds_union_with_negative_chunks)
{
	World w;
	w.Set(-1, -1, -1, BLOCK_DIRT);   // chunk (-1,-1,-1): cells -16..-1
	w.Set(40, 0, 0, BLOCK_DIRT);     // chunk (2,0,0): cells 32..47
	int x0, y0, z0, x1, y1, z1;
	MC_CHECK(w.GetBounds(x0, y0, z0, x1, y1, z1));
	MC_CHECK_EQ(x0, -16); MC_CHECK_EQ(y0, -16); MC_CHECK_EQ(z0, -16);
	MC_CHECK_EQ(x1, 47); MC_CHECK_EQ(y1, 15); MC_CHECK_EQ(z1, 15);
}

MC_TEST(bounds_shrinks_when_chunk_is_freed)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);
	w.Set(100, 0, 0, BLOCK_DIRT);
	w.Set(100, 0, 0, BLOCK_AIR);
	int x0, y0, z0, x1, y1, z1;
	MC_CHECK(w.GetBounds(x0, y0, z0, x1, y1, z1));
	MC_CHECK_EQ(x1, 15);
}
```
- [ ] **Step 2: Run to verify it fails** — `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j` → compile error (`GetBounds` not a member).
- [ ] **Step 3: Implement** — declare in `McWorld.h` (public, next to `ChunkCount`): `bool GetBounds(int &minX, int &minY, int &minZ, int &maxX, int &maxY, int &maxZ) const;` and in `McWorld.cpp`:
```cpp
bool World::GetBounds(int &minX, int &minY, int &minZ, int &maxX, int &maxY, int &maxZ) const
{
	if(m_chunks.empty())
		return false;
	bool first = true;
	for(ChunkMap::const_iterator it = m_chunks.begin(); it != m_chunks.end(); ++it){
		int x0 = it->first.x * CHUNK_SIZE, y0 = it->first.y * CHUNK_SIZE, z0 = it->first.z * CHUNK_SIZE;
		int x1 = x0 + CHUNK_SIZE - 1, y1 = y0 + CHUNK_SIZE - 1, z1 = z0 + CHUNK_SIZE - 1;
		if(first){
			minX = x0; minY = y0; minZ = z0;
			maxX = x1; maxY = y1; maxZ = z1;
			first = false;
		}else{
			if(x0 < minX) minX = x0;
			if(y0 < minY) minY = y0;
			if(z0 < minZ) minZ = z0;
			if(x1 > maxX) maxX = x1;
			if(y1 > maxY) maxY = y1;
			if(z1 > maxZ) maxZ = z1;
		}
	}
	return true;
}
```
- [ ] **Step 4: Run to verify it passes** — `cmake --build build/mctest -j && build/mctest/mctests` → all pass, no `-Wall -Wextra` warnings.
- [ ] **Step 5: Commit** — `git add tests/minecraft/test_bounds.cpp src/minecraft/core/McWorld.h src/minecraft/core/McWorld.cpp` ; subject `feat(minecraft): world bounds for culling`.

---

### Task 2: Oriented box vs blocks (`McOrientedBox`)

**Files:** Create `src/minecraft/core/McOrientedBox.h`, `src/minecraft/core/McOrientedBox.cpp`, `tests/minecraft/test_obox.cpp`.

**Interfaces:**
- Consumes: `World::Get`, `BLOCK_AIR`.
- Produces (namespace `Mc`):
  - `struct OrientedBox { float x, y; float zBottom, zTop; float halfU, halfV; float yaw; }` — horizontal centre `(x,y)`, vertical range `[zBottom, zTop]`, half extents along the local axes `u = (cos yaw, sin yaw)` and `v = (-sin yaw, cos yaw)`.
  - `bool BoxOverlapsBlocks(const World &w, const OrientedBox &b)` — true if the box strictly overlaps (more than `EPS`) any non-air cell.
  - `struct BoxPush { bool moved; bool onTop; float dx, dy, dz; }` and `BoxPush PushBoxOutOfBlocks(const World &w, OrientedBox &b)` — pushes the box out along the axis of minimal penetration (candidate axes: world X, world Y, u, v, world Z) over at most 8 iterations; `dx,dy,dz` is the total translation applied; `onTop` is true if any push was upward (+Z).

- [ ] **Step 1: Write the failing tests** — `tests/minecraft/test_obox.cpp`:
```cpp
#include "mctest.h"
#include "McOrientedBox.h"
#include <math.h>

using namespace Mc;

static OrientedBox MakeBox(float x, float y, float zb, float zt, float hu, float hv, float yaw)
{
	OrientedBox b = { x, y, zb, zt, hu, hv, yaw };
	return b;
}

static void Wall(World &w, int x, int y0, int y1, int z0, int z1)
{
	for(int y = y0; y <= y1; y++)
		for(int z = z0; z <= z1; z++)
			w.Set(x, y, z, BLOCK_STONE);
}

MC_TEST(obox_empty_world_does_not_overlap)
{
	World w;
	OrientedBox b = MakeBox(0, 0, 0, 1, 2, 1, 0.3f);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(!p.moved);
}

MC_TEST(obox_yaw0_wall_pushes_back_along_x)
{
	World w;
	Wall(w, 5, -3, 3, 0, 0);
	// box x range 1.5..5.5 overlaps wall cells (x from 5) by 0.5; y range +-0.9 within the wall
	OrientedBox b = MakeBox(3.5f, 0.0f, 0.0f, 1.0f, 2.0f, 0.9f, 0.0f);
	MC_CHECK(BoxOverlapsBlocks(w, b));
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(!p.onTop);
	MC_CHECK_NEAR(b.x, 3.0, 1e-3);
	MC_CHECK_NEAR(b.y, 0.0, 1e-3);
	MC_CHECK_NEAR(p.dx, -0.5, 1e-3);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
}

MC_TEST(obox_yaw90_uses_rotated_extents)
{
	World w;
	Wall(w, 5, -3, 3, 0, 0);
	// yaw 90 deg: u points along +Y (half 2.0), v along -X (half 0.9); x range 3.6..5.4 overlaps by 0.4
	OrientedBox b = MakeBox(4.5f, 0.0f, 0.0f, 1.0f, 2.0f, 0.9f, 1.57079633f);
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK_NEAR(b.x, 4.1, 1e-3);
	MC_CHECK_NEAR(b.y, 0.0, 1e-3);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
}

MC_TEST(obox_yaw45_is_resolved_without_overlap)
{
	World w;
	Wall(w, 5, -6, 6, 0, 0);
	OrientedBox b = MakeBox(3.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f, 0.78539816f);
	MC_CHECK(BoxOverlapsBlocks(w, b));
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
	MC_CHECK(b.x < 3.0f);                 // moved away from the wall
	float d = sqrtf(p.dx * p.dx + p.dy * p.dy + p.dz * p.dz);
	MC_CHECK(d < 1.0f);                   // a small correction, not a teleport
}

MC_TEST(obox_floor_pushes_up_and_reports_on_top)
{
	World w;
	for(int x = -5; x <= 5; x++)
		for(int y = -5; y <= 5; y++)
			w.Set(x, y, 0, BLOCK_STONE);   // floor top at z = 1
	OrientedBox b = MakeBox(0.5f, 0.5f, 0.9f, 2.7f, 1.0f, 1.0f, 0.3f);
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(p.onTop);
	MC_CHECK_NEAR(b.zBottom, 1.0, 1e-3);
	MC_CHECK_NEAR(b.zTop, 2.8, 1e-3);
	MC_CHECK_NEAR(p.dz, 0.1, 1e-3);
}

MC_TEST(obox_ceiling_pushes_down)
{
	World w;
	for(int x = -5; x <= 5; x++)
		for(int y = -5; y <= 5; y++)
			w.Set(x, y, 3, BLOCK_STONE);   // ceiling bottom at z = 3
	OrientedBox b = MakeBox(0.5f, 0.5f, 1.3f, 3.1f, 1.0f, 1.0f, 0.0f);
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(!p.onTop);
	MC_CHECK_NEAR(b.zTop, 3.0, 1e-3);
}

MC_TEST(obox_touching_faces_do_not_overlap)
{
	World w;
	for(int x = -5; x <= 5; x++)
		for(int y = -5; y <= 5; y++)
			w.Set(x, y, 0, BLOCK_STONE);
	OrientedBox b = MakeBox(0.5f, 0.5f, 1.0f, 2.8f, 1.0f, 1.0f, 0.7f);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
	MC_CHECK(!PushBoxOutOfBlocks(w, b).moved);
}

MC_TEST(obox_negative_coordinates)
{
	World w;
	for(int x = -10; x <= -6; x++)
		for(int y = -10; y <= -6; y++)
			w.Set(x, y, -5, BLOCK_DIRT);    // floor top at z = -4
	OrientedBox b = MakeBox(-7.5f, -8.5f, -4.1f, -2.3f, 1.0f, 0.8f, -0.4f);
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(p.onTop);
	MC_CHECK_NEAR(b.zBottom, -4.0, 1e-3);
}

MC_TEST(obox_inside_solid_terminates)
{
	World w;
	for(int x = -3; x <= 3; x++)
		for(int y = -3; y <= 3; y++)
			for(int z = -3; z <= 3; z++)
				w.Set(x, y, z, BLOCK_STONE);
	OrientedBox b = MakeBox(0.0f, 0.0f, -0.5f, 0.5f, 1.0f, 1.0f, 0.2f);
	BoxPush p = PushBoxOutOfBlocks(w, b);          // must return (8 iteration cap), whatever it does
	MC_CHECK(p.moved);
	MC_CHECK(sqrtf(p.dx * p.dx + p.dy * p.dy + p.dz * p.dz) < 40.0f);
}
```
- [ ] **Step 2: Run to verify it fails** — compile error `McOrientedBox.h: No such file`.
- [ ] **Step 3: Implement**

`src/minecraft/core/McOrientedBox.h`:
```cpp
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
```

`src/minecraft/core/McOrientedBox.cpp`:
```cpp
#include "McOrientedBox.h"
#include <math.h>

namespace Mc {

static const float EPS = 1e-4f;

struct Hit {
	bool overlap;
	float depth;
	int axis;	// 0 X, 1 Y, 2 u, 3 v, 4 Z
	float sign;	// push direction along the axis
};

// SAT between the box and one cell: world X, Y, the box's u and v (horizontal), and Z.
static Hit TestCell(const OrientedBox &b, int cx, int cy, int cz)
{
	Hit best = { false, 1e30f, -1, 1.0f };
	float c = cosf(b.yaw), s = sinf(b.yaw);
	float ax[4] = { 1.0f, 0.0f, c, -s };
	float ay[4] = { 0.0f, 1.0f, s, c };
	float cellX = (float)cx + 0.5f, cellY = (float)cy + 0.5f;

	for(int i = 0; i < 4; i++){
		float uDot = c * ax[i] + s * ay[i];		// u . a
		float vDot = -s * ax[i] + c * ay[i];		// v . a
		float r = b.halfU * fabsf(uDot) + b.halfV * fabsf(vDot);
		float cellR = 0.5f * (fabsf(ax[i]) + fabsf(ay[i]));
		float d = (b.x * ax[i] + b.y * ay[i]) - (cellX * ax[i] + cellY * ay[i]);
		float overlap = r + cellR - fabsf(d);
		if(overlap <= EPS)
			return best;			// separated on this axis
		if(overlap < best.depth){
			best.depth = overlap;
			best.axis = i;
			best.sign = d >= 0.0f ? 1.0f : -1.0f;
		}
	}
	float mid = 0.5f * (b.zBottom + b.zTop), half = 0.5f * (b.zTop - b.zBottom);
	float d = mid - ((float)cz + 0.5f);
	float overlap = half + 0.5f - fabsf(d);
	if(overlap <= EPS)
		return best;
	if(overlap < best.depth){
		best.depth = overlap;
		best.axis = 4;
		best.sign = d >= 0.0f ? 1.0f : -1.0f;
	}
	best.overlap = true;
	return best;
}

static void CellRange(const OrientedBox &b, int &x0, int &y0, int &z0, int &x1, int &y1, int &z1)
{
	float c = fabsf(cosf(b.yaw)), s = fabsf(sinf(b.yaw));
	float rx = c * b.halfU + s * b.halfV;
	float ry = s * b.halfU + c * b.halfV;
	x0 = (int)floorf(b.x - rx + EPS);
	x1 = (int)floorf(b.x + rx - EPS);
	y0 = (int)floorf(b.y - ry + EPS);
	y1 = (int)floorf(b.y + ry - EPS);
	z0 = (int)floorf(b.zBottom + EPS);
	z1 = (int)floorf(b.zTop - EPS);
}

bool BoxOverlapsBlocks(const World &w, const OrientedBox &b)
{
	int x0, y0, z0, x1, y1, z1;
	CellRange(b, x0, y0, z0, x1, y1, z1);
	for(int z = z0; z <= z1; z++)
	for(int y = y0; y <= y1; y++)
	for(int x = x0; x <= x1; x++)
		if(w.Get(x, y, z) != BLOCK_AIR && TestCell(b, x, y, z).overlap)
			return true;
	return false;
}

BoxPush PushBoxOutOfBlocks(const World &w, OrientedBox &b)
{
	BoxPush res = { false, false, 0.0f, 0.0f, 0.0f };
	for(int iter = 0; iter < 8; iter++){
		int x0, y0, z0, x1, y1, z1;
		CellRange(b, x0, y0, z0, x1, y1, z1);
		bool pushed = false;
		for(int z = z0; z <= z1 && !pushed; z++)
		for(int y = y0; y <= y1 && !pushed; y++)
		for(int x = x0; x <= x1 && !pushed; x++){
			if(w.Get(x, y, z) == BLOCK_AIR)
				continue;
			Hit h = TestCell(b, x, y, z);
			if(!h.overlap)
				continue;
			float c = cosf(b.yaw), s = sinf(b.yaw);
			float dx = 0.0f, dy = 0.0f, dz = 0.0f;
			float m = h.depth * h.sign;
			switch(h.axis){
			case 0: dx = m; break;
			case 1: dy = m; break;
			case 2: dx = c * m; dy = s * m; break;
			case 3: dx = -s * m; dy = c * m; break;
			default: dz = m; break;
			}
			b.x += dx; b.y += dy;
			b.zBottom += dz; b.zTop += dz;
			res.dx += dx; res.dy += dy; res.dz += dz;
			res.moved = true;
			if(h.axis == 4 && h.sign > 0.0f)
				res.onTop = true;
			pushed = true;
		}
		if(!pushed)
			break;
	}
	return res;
}

}
```
- [ ] **Step 4: Run to verify it passes** — `cmake --build build/mctest -j && build/mctest/mctests` → all pass, no warnings. If `obox_yaw45_is_resolved_without_overlap` fails because 8 iterations are not enough, report it with numbers: do not weaken the test without a recorded ruling.
- [ ] **Step 5: Commit** — stage the 3 new files; subject `feat(minecraft): oriented box vs blocks push-out`.

---

### Task 3: `SweepBox`

**Files:** Modify `src/minecraft/core/McOrientedBox.h`, `src/minecraft/core/McOrientedBox.cpp`; Create `tests/minecraft/test_sweep.cpp`.

**Interfaces:**
- Consumes: `OrientedBox`, `BoxOverlapsBlocks`.
- Produces: `struct SweepResult { bool blocked; float x, y, dz; }` and `SweepResult SweepBox(const World &w, const OrientedBox &from, float dx, float dy, float dz)` — moves the box by `(dx,dy,dz)` in steps of at most 0.4; returns the last pose before the first overlap (`x,y` = new centre, `dz` = vertical shift applied) with `blocked = true`, or the target pose with `blocked = false`. If the starting box already overlaps blocks, nothing is swept: returns the target pose with `blocked = false`.

- [ ] **Step 1: Write the failing tests** — `tests/minecraft/test_sweep.cpp`:
```cpp
#include "mctest.h"
#include "McOrientedBox.h"

using namespace Mc;

static World *WallWorld(World &w)
{
	for(int y = -6; y <= 6; y++)
		for(int z = 0; z <= 1; z++)
			w.Set(5, y, z, BLOCK_STONE);	// wall at x in [5,6)
	return &w;
}

static OrientedBox Car(float x)
{
	OrientedBox b = { x, 0.0f, 0.0f, 1.5f, 1.0f, 0.9f, 0.0f };	// x extent +-1 (u along +X)
	return b;
}

MC_TEST(sweep_fast_mover_is_stopped_by_wall)
{
	World w;
	WallWorld(w);
	SweepResult r = SweepBox(w, Car(0.0f), 100.0f, 0.0f, 0.0f);	// 100 m in one frame
	MC_CHECK(r.blocked);
	MC_CHECK(r.x >= 3.5f && r.x <= 4.0f + 1e-3f);		// box max x = x+1 must stay <= 5
	MC_CHECK_NEAR(r.dz, 0.0, 1e-6);
}

MC_TEST(sweep_one_metre_step_does_not_tunnel_through_one_block_wall)
{
	World w;
	WallWorld(w);
	// box at x=3.9 (max 4.9) moves +3: its target (6.9) is past the 1-block wall
	SweepResult r = SweepBox(w, Car(3.9f), 3.0f, 0.0f, 0.0f);
	MC_CHECK(r.blocked);
	MC_CHECK(r.x <= 4.0f + 1e-3f);
}

MC_TEST(sweep_no_wall_reaches_target)
{
	World w;
	SweepResult r = SweepBox(w, Car(0.0f), 12.0f, 3.0f, 0.5f);
	MC_CHECK(!r.blocked);
	MC_CHECK_NEAR(r.x, 12.0, 1e-4);
	MC_CHECK_NEAR(r.y, 3.0, 1e-4);
	MC_CHECK_NEAR(r.dz, 0.5, 1e-4);
}

MC_TEST(sweep_parallel_to_wall_is_not_blocked)
{
	World w;
	WallWorld(w);
	OrientedBox b = Car(0.0f);			// x in [-1,1], far from the wall
	SweepResult r = SweepBox(w, b, 0.0f, 5.0f, 0.0f);
	MC_CHECK(!r.blocked);
}

MC_TEST(sweep_starting_inside_blocks_returns_target)
{
	World w;
	WallWorld(w);
	OrientedBox b = Car(5.5f);			// already inside the wall
	SweepResult r = SweepBox(w, b, 2.0f, 0.0f, 0.0f);
	MC_CHECK(!r.blocked);
	MC_CHECK_NEAR(r.x, 7.5, 1e-4);
}

MC_TEST(sweep_negative_direction_and_coordinates)
{
	World w;
	for(int y = -6; y <= 6; y++)
		for(int z = 0; z <= 1; z++)
			w.Set(-6, y, z, BLOCK_STONE);	// wall at x in [-6,-5)
	SweepResult r = SweepBox(w, Car(0.0f), -100.0f, 0.0f, 0.0f);
	MC_CHECK(r.blocked);
	MC_CHECK(r.x <= -4.0f + 1e-3f + 0.4f && r.x >= -4.0f - 1e-3f);	// box min x = x-1 must stay >= -5
}

MC_TEST(sweep_zero_move_is_not_blocked)
{
	World w;
	WallWorld(w);
	SweepResult r = SweepBox(w, Car(0.0f), 0.0f, 0.0f, 0.0f);
	MC_CHECK(!r.blocked);
	MC_CHECK_NEAR(r.x, 0.0, 1e-6);
}
```
- [ ] **Step 2: Run to verify it fails** — compile error (`SweepBox`/`SweepResult` undeclared).
- [ ] **Step 3: Implement** — append to `McOrientedBox.h` (inside `namespace Mc`):
```cpp
struct SweepResult {
	bool blocked;
	float x, y, dz;	// last free centre and the vertical shift applied
};

// Moves the box by (dx,dy,dz) in steps of at most 0.4 and stops before the first overlap.
SweepResult SweepBox(const World &w, const OrientedBox &from, float dx, float dy, float dz);
```
and in `McOrientedBox.cpp`:
```cpp
SweepResult SweepBox(const World &w, const OrientedBox &from, float dx, float dy, float dz)
{
	SweepResult res = { false, from.x + dx, from.y + dy, dz };
	if(BoxOverlapsBlocks(w, from))
		return res;
	float dist = sqrtf(dx * dx + dy * dy + dz * dz);
	int steps = (int)ceilf(dist / 0.4f);
	if(steps < 1)
		return res;
	if(steps > 1000)
		steps = 1000;
	for(int i = 1; i <= steps; i++){
		float t = (float)i / (float)steps;
		OrientedBox b = from;
		b.x += dx * t;
		b.y += dy * t;
		b.zBottom += dz * t;
		b.zTop += dz * t;
		if(BoxOverlapsBlocks(w, b)){
			float p = (float)(i - 1) / (float)steps;
			res.blocked = true;
			res.x = from.x + dx * p;
			res.y = from.y + dy * p;
			res.dz = dz * p;
			return res;
		}
	}
	return res;
}
```
Note: the 1000-step cap bounds the work for absurd displacements; a displacement above 400 m is clamped to steps of `dist/1000` (documented in the code comment).
- [ ] **Step 4: Run to verify it passes** — all pass, no warnings.
- [ ] **Step 5: Commit** — stage the changed/new files; subject `feat(minecraft): swept box test against blocks`.

---

### Task 4: `McEntities` — peds

**Files:** Create `src/minecraft/game/McEntities.h`, `src/minecraft/game/McEntities.cpp`; Modify `src/minecraft/game/McMode.cpp`.

**Interfaces:**
- Consumes: `Mc::World::GetBounds`, `Mc::PushOutOfBlocks` (from `McCollide.h`), `McInteract.h` body constants (`bodyFeetOffset`, `bodyHalfWidth`, `bodyHeight`), `CPools::GetPedPool()` (`core/Pools.h`), `FindPlayerPed()`.
- Produces: `McEntities::Update(Mc::World &world)` — called every frame from `McMode::Update` after the existing player handling, whether or not Steve mode is active, and only does work when the world has blocks.

Behaviour (spec "Peds"): compute `GetBounds` once per call (return immediately if false). For each ped in the ped pool: skip nil, the player ped, peds in a vehicle (`bInVehicle`), peds with `!bUsesCollision`, and peds whose position is outside the voxel bounds expanded by 2 cells (z by 3). Then run `Mc::PushOutOfBlocks` on the same body box the player uses (feet = origin - `bodyFeetOffset`). If it moved: `ped->SetPosition(...)`; remove the velocity component along the push direction when it points into the block (for a push `(dx,dy,dz)` normalise it to n; if dot(velocity, n) < 0 then velocity -= dot * n); if the push was upward (`onGround`): `bIsStanding = true`, `m_vecMoveSpeed.z = 0` and, if `bIsInTheAir`, call `ped->SetLanding()`. Dead peds (`DyingOrDead()`) are skipped.

- [ ] **Step 1: Verify the APIs** (grep, record real names in your report if they differ): `grep -n "GetPedPool\|GetVehiclePool" src/core/Pools.h`; `grep -n "bInVehicle\|bUsesCollision" src/peds/Ped.h src/entities/Entity.h`; `grep -n "DyingOrDead\|SetLanding\|bIsInTheAir" src/peds/Ped.h`; look at how `McMode.cpp` `UpdateGround` and the player push-out use `ped->SetPosition`, `m_vecMoveSpeed`, `bIsStanding`, `SetLanding`; and what `CPool::GetSize()/GetSlot(i)` return (a null pointer for free slots).
- [ ] **Step 2: Implement** `McEntities.h`:
```cpp
#pragma once

#include "McWorld.h"

namespace McEntities
{
	void Update(Mc::World &world);
}
```
`McEntities.cpp` (adapt includes to what compiles; same order trick as `McMode.cpp`):
```cpp
#include "common.h"

#ifdef MINECRAFT_MODE

#include "Ped.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "World.h"
#include "Pools.h"
#include "McEntities.h"
#include "McInteract.h"
#include "McCollide.h"

namespace McEntities
{

static bool
NearBounds(const CVector &p, int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
	return p.x >= minX - 2 && p.x <= maxX + 3 &&
		p.y >= minY - 2 && p.y <= maxY + 3 &&
		p.z >= minZ - 3 && p.z <= maxZ + 4;
}

static void
CancelVelocityInto(CVector &vel, float dx, float dy, float dz)
{
	float len = sqrtf(dx * dx + dy * dy + dz * dz);
	if(len < 1e-6f)
		return;
	CVector n(dx / len, dy / len, dz / len);
	float d = DotProduct(vel, n);
	if(d < 0.0f)
		vel -= n * d;
}

static void
UpdatePeds(Mc::World &world, int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
	CPool<CPed, CPlayerPed> *pool = CPools::GetPedPool();
	for(int i = 0; i < pool->GetSize(); i++){
		CPed *ped = pool->GetSlot(i);
		if(ped == nil || ped == FindPlayerPed() || ped->bInVehicle || !ped->bUsesCollision || ped->DyingOrDead())
			continue;
		CVector pos = ped->GetPosition();
		if(!NearBounds(pos, minX, minY, minZ, maxX, maxY, maxZ))
			continue;
		float fx = pos.x, fy = pos.y, fz = pos.z - bodyFeetOffset;
		Mc::CollideResult r = Mc::PushOutOfBlocks(world, fx, fy, fz, bodyHalfWidth, bodyHeight);
		if(!r.moved)
			continue;
		float dx = fx - pos.x, dy = fy - pos.y, dz = (fz + bodyFeetOffset) - pos.z;
		ped->SetPosition(fx, fy, fz + bodyFeetOffset);
		CancelVelocityInto(ped->m_vecMoveSpeed, dx, dy, dz);
		if(r.onGround){
			ped->bIsStanding = true;
			ped->m_vecMoveSpeed.z = 0.0f;
			if(ped->bIsInTheAir)
				ped->SetLanding();
		}
	}
}

void
Update(Mc::World &world)
{
	int minX, minY, minZ, maxX, maxY, maxZ;
	if(!world.GetBounds(minX, minY, minZ, maxX, maxY, maxZ))
		return;
	UpdatePeds(world, minX, minY, minZ, maxX, maxY, maxZ);
}

}

#endif
```
(`bodyFeetOffset`, `bodyHalfWidth`, `bodyHeight` are in `McInteract.h`: check their namespace and qualify them accordingly. `CPool` template args and `DotProduct`/`operator-=` on `CVector` must match what exists; fix compile errors by reading the headers.)
- [ ] **Step 3: Wire it in** — in `McMode.cpp` add `#include "McEntities.h"` and at the end of `McMode::Update`, after the player code and unconditionally (not inside the `if(active)` blocks): `McEntities::Update(world);`.
- [ ] **Step 4: Build** — `cd build && cmake . && cmake --build . -j$(nproc)`: no `error`, final `Linking CXX executable src/re3`, no new warnings from `src/minecraft`. Re-run the standalone tests (unchanged count, all pass).
- [ ] **Step 5: Commit** — `git add src/minecraft/game/McEntities.h src/minecraft/game/McEntities.cpp src/minecraft/game/McMode.cpp`; subject `feat(minecraft): peds collide with blocks`.
- Manual in-game check (user, later): build a wall in a street; pedestrians walking into it stop; a ped pushed up onto a block stands without the fall animation.

---

### Task 5: `McEntities` — vehicles

**Files:** Modify `src/minecraft/game/McEntities.cpp`.

**Interfaces:**
- Consumes: Task 2/3 (`OrientedBox`, `PushBoxOutOfBlocks`, `SweepBox`), `CPools::GetVehiclePool()`, `CVehicle::GetColModel()->boundingBox` (local `min`/`max`), the vehicle matrix (`GetMatrix()`, `GetForward()`), `m_vecMoveSpeed`.
- Produces: vehicles handled in `McEntities::Update` after peds.

Behaviour (spec "Vehicles"): only cars (`GetVehicleType() == VEHICLE_TYPE_CAR`; skip boats, trains, helicopters, planes: verify the exact enum by grep); skip nil, `!bUsesCollision`, entities outside the expanded bounds. Build the oriented box from the colmodel `boundingBox`: local centre `lc = (min+max)/2`, `halfU = (max.y-min.y)/2` (car length, local Y is the forward axis), `halfV = (max.x-min.x)/2`, `halfZ = (max.z-min.z)/2`; world centre `c = pos + matrix * lc` (use the matrix's rotation: `GetMatrix() * lc` yields the world position of a local point); `yaw = atan2f(forward.y, forward.x)` with `u = forward`; `zBottom = c.z - halfZ`, `zTop = c.z + halfZ`. Per-entity previous position is kept in a small `std::unordered_map<const CVehicle*, CVector>` (the vehicle origin from the previous frame, recorded after resolution); entries for vehicles not visited this frame are dropped at the end of the frame (build a new map each frame and swap). If a previous position exists and the origin moved more than 0.3 since: `SweepBox` from the box shifted back by that displacement to the current box centre; if `blocked`, set the vehicle origin to `pos + (lastFree - currentCentre)` (for x, y, z), and cancel the velocity component along the travel direction (restitution 0.2: `vel -= 1.2 * dot(vel, d) * d` when `dot > 0`). Then run `PushBoxOutOfBlocks` on the (possibly corrected) box and apply its translation `(dx,dy,dz)` to the vehicle origin; cancel the velocity component into the push direction with the same restitution; if `onTop`, also set `m_vecMoveSpeed.z = 0` (a car resting on top of a block is held there; wheels see no ground: accepted limitation, document it in a comment). If the player is in the vehicle, the same code applies.

- [ ] **Step 1: Verify the APIs**: `grep -n "boundingBox" src/collision/ColModel.h`; `grep -n "VEHICLE_TYPE_CAR\|GetVehicleType\|m_vehType" src/vehicles/Vehicle.h`; how a `CMatrix` multiplies a `CVector` (`operator*(const CMatrix&, const CVector&)` in `src/math/Matrix.h`/`Vector.h`); `SetPosition` on vehicles (CPlaceable); the member names for the movement speed. Record any difference in your report.
- [ ] **Step 2: Implement** `UpdateVehicles(...)` in `McEntities.cpp` following the behaviour above, structured as small helpers (one for building the box from a vehicle, one for the velocity response), and call it from `Update` after `UpdatePeds`. Include `McOrientedBox.h`, `Vehicle.h` (and `<unordered_map>`); keep the file focused (split into `McEntities.cpp` and a separate `McVehicles.cpp` only if `McEntities.cpp` grows beyond ~250 lines).
- [ ] **Step 3: Build** — same commands as Task 4; no errors, no new warnings from `src/minecraft`; standalone tests still all pass.
- [ ] **Step 4: Commit** — `git add src/minecraft/game/McEntities.cpp` (and any new file); subject `feat(minecraft): cars collide with blocks`.
- Manual in-game check (user, later): a wall across a road and a car driven into it at speed (should stop, not tunnel); a parked car beside blocks; the player's own car against a wall; a car pushed against a 1-block wall from different angles (no flipping, no jitter).

---

## Self-Review

**Spec coverage:** bounds (Task 1), oriented box push-out (2), sweep (3), peds (4), vehicles incl. the player's car and previous-pose sweep (5), no Rockstar files changed (all tasks), tests in `tests/minecraft` (1-3). Out of scope exactly as in the spec.

**Type consistency:** `OrientedBox`, `BoxPush`, `SweepResult`, `GetBounds(int&...)` names are identical across tasks 1-5. `bodyFeetOffset`/`bodyHalfWidth`/`bodyHeight` come from the existing `McInteract.h`.

**Known risks for the executor:** (1) 8 push iterations may not resolve a box wedged diagonally into a corner: record it instead of loosening tests; (2) vehicles can flip or jitter if the velocity response is too strong: keep restitution at 0.2 and do not touch turn speed; (3) AI peds and cars keep trying to walk or drive into blocks (no avoidance): expected.
