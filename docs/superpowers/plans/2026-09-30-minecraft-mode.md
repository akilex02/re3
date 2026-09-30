# Minecraft mode for re3 — Implementation Plan (plan 1 of 2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A toggleable "Steve mode" in re3 where the player places and breaks voxel blocks in the GTA III world (flat-coloured), collides with them, and melee-hits peds.

**Architecture:** A pure-C++ core (`src/minecraft/core/`: world, blocks, raycast, mesher, collision) with no GTA dependency, unit-tested outside the game. A thin game layer (`src/minecraft/game/`) hooks into re3 through four one-line calls behind `MINECRAFT_MODE`. Rendering uses `RwIm3D` inside `RenderEffects()`.

**Tech Stack:** C++11 (re3 builds with `CXX_STANDARD 11`), librw via re3's fakerw shim, CMake/Ninja, a tiny in-repo test runner (no external test framework).

**Spec:** `docs/superpowers/specs/2026-09-30-minecraft-mode-design.md`

**Out of this plan (follow-up plan 2):** `McAtlas` (Mojang texture download, needs a PNG/zip path), the Steve model/skin, Minecraft-tuned jump/sprint physics, hotbar HUD, and phase 1b (peds/vehicles colliding with blocks). This plan uses the spec's flat-colour fallback and prints the selected block to stdout.

## Global Constraints

- C++11 only (`CXX_STANDARD 11`). No `std::make_unique`, no `auto` return deduction, no structured bindings.
- `src/CMakeLists.txt` globs `*.cpp`/`*.h` under `src/` recursively and adds every directory containing sources to the include path. Headers are included by bare name (`#include "McWorld.h"`). Never put test files under `src/`, or they get linked into the game.
- Never edit `vendor/`. Do not touch the uncommitted changes in `src/core/config.h` beyond the one `MINECRAFT_MODE` block added in Task 5.
- Core units (`src/minecraft/core/`) include only the standard library. They must compile without `common.h`. Use `<stdint.h>` types there (the project typedefs `int32`/`uint8` are for the game layer).
- Game-layer code follows `CODING_STYLE.md`: tabs, return type on its own line, brace on the next line for function definitions, no braces around single statements, `int *ptr`.
- GTA axes: X east, Y north, **Z up**. 1 block = 1 GTA unit. Block `(x,y,z)` occupies `[x,x+1) x [y,y+1) x [z,z+1)`.
- Every change to Rockstar code is behind `#ifdef MINECRAFT_MODE`. With the flag off, re3 must behave exactly as before.
- The toggle key is F8 (`CPad::GetPad(0)->GetFJustDown(7)`; F1..F12 map to indices 0..11; index 0 is used only in replays, index 11 by `main.cpp`, index 7 is unused).
- Save file: `mcworld.dat` in the game's working directory. Magic `MCW1`.
- Commit only when the user asks. Steps titled "Commit" below are for the executor to perform only if the user has approved committing for this work; otherwise skip them.

## Review Focus

Inputs the spec implies but the core tests would not otherwise exercise, most likely first:

- Negative coordinates (Liberty City spans negative X/Y): `Get/Set`, chunk indexing and raycast must behave identically for negative cells (tested in Tasks 1, 3).
- Setting a block to air in a chunk that does not exist must not allocate a chunk (Task 1).
- A corrupt, truncated or wrong-magic `mcworld.dat` must load as "failed, world left empty" without crashing (Task 2).
- Placing a block on the face the ray hit when the ray starts inside a block (normal `0,0,0`) must be rejected, not placed at the hit cell (Task 7).
- The worst-case chunk (checkerboard) must fit 16-bit mesh indices (Task 4).

## File Structure

```
tests/minecraft/CMakeLists.txt        standalone test build (not part of re3's build)
tests/minecraft/mctest.h              MC_TEST / MC_CHECK macros
tests/minecraft/mctest_main.cpp       runner, optional name filter in argv[1]
tests/minecraft/test_world.cpp        Tasks 1-2
tests/minecraft/test_ray.cpp          Task 3
tests/minecraft/test_mesher.cpp       Task 4
tests/minecraft/test_collide.cpp      Task 8
src/minecraft/core/McBlocks.h/.cpp    block ids, colours, transparency
src/minecraft/core/McWorld.h/.cpp     sparse chunks, Get/Set, dirty flags, save/load
src/minecraft/core/McRay.h/.cpp       voxel DDA raycast
src/minecraft/core/McMesher.h/.cpp    chunk to vertices/indices
src/minecraft/core/McCollide.h/.cpp   AABB vs voxel push-out
src/minecraft/game/McRenderer.h/.cpp  RwIm3D drawing of chunk meshes
src/minecraft/game/McInteract.h/.cpp  camera ray, place, break, melee
src/minecraft/game/McMode.h/.cpp      toggle, lifecycle, the only GTA touch point
```

Run tests with:
```
cmake -S tests/minecraft -B build/mctest -DCMAKE_BUILD_TYPE=Debug && cmake --build build/mctest -j && build/mctest/mctests
```
(run from `/home/akilex/Descargas/gtas/re3`). A single test: `build/mctest/mctests <name-substring>`.

---

### Task 1: Test harness, block table and McWorld get/set

**Files:**
- Create: `tests/minecraft/CMakeLists.txt`, `tests/minecraft/mctest.h`, `tests/minecraft/mctest_main.cpp`, `tests/minecraft/test_world.cpp`
- Create: `src/minecraft/core/McBlocks.h`, `src/minecraft/core/McBlocks.cpp`, `src/minecraft/core/McWorld.h`, `src/minecraft/core/McWorld.cpp`

**Interfaces:**
- Produces (namespace `Mc`):
  - `enum BlockId : uint8_t { BLOCK_AIR = 0, BLOCK_DIRT, BLOCK_STONE, BLOCK_WOOD, BLOCK_GLASS, BLOCK_COUNT }`
  - `struct BlockInfo { const char *name; uint8_t r, g, b; bool transparent; }`; `const BlockInfo &GetBlockInfo(uint8_t id)` (ids >= `BLOCK_COUNT` return the air entry)
  - `const int CHUNK_SIZE = 16`; `inline int ChunkIndex(int lx, int ly, int lz)` = `lx + 16 * (ly + 16 * lz)`
  - `int FloorDiv(int a, int b)`, `int FloorMod(int a, int b)` (b > 0, floor semantics for negatives)
  - `struct ChunkPos { int32_t x, y, z; }` with `operator==` and `struct ChunkPosHash`
  - `struct Chunk { uint8_t blocks[4096]; int count; bool dirty; }`
  - `class World { uint8_t Get(int x,int y,int z) const; bool Set(int x,int y,int z,uint8_t id); size_t ChunkCount() const; Chunk *FindChunk(ChunkPos); const Chunk *FindChunk(ChunkPos) const; const std::unordered_map<ChunkPos, Chunk*, ChunkPosHash> &Chunks() const; void Clear(); ~World(); }` (World owns the `Chunk*` and is non-copyable)

- [ ] **Step 1: Create the test harness**

`tests/minecraft/mctest.h`:
```cpp
#pragma once
#include <stdio.h>
#include <stdlib.h>

typedef void (*McTestFn)(void);
void McTestRegister(const char *name, McTestFn fn);

struct McTestReg {
	McTestReg(const char *name, McTestFn fn) { McTestRegister(name, fn); }
};

#define MC_TEST(name) \
	static void name(void); \
	static McTestReg reg_##name(#name, name); \
	static void name(void)

#define MC_CHECK(cond) \
	do { \
		if(!(cond)){ \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			exit(1); \
		} \
	} while(0)

#define MC_CHECK_EQ(a, b) \
	do { \
		long long va_ = (long long)(a), vb_ = (long long)(b); \
		if(va_ != vb_){ \
			fprintf(stderr, "%s:%d: CHECK_EQ failed: %s (%lld) != %s (%lld)\n", __FILE__, __LINE__, #a, va_, #b, vb_); \
			exit(1); \
		} \
	} while(0)

#define MC_CHECK_NEAR(a, b, eps) \
	do { \
		double va_ = (double)(a), vb_ = (double)(b); \
		if(va_ - vb_ > (eps) || vb_ - va_ > (eps)){ \
			fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s (%f) vs %s (%f)\n", __FILE__, __LINE__, #a, va_, #b, vb_); \
			exit(1); \
		} \
	} while(0)
```

`tests/minecraft/mctest_main.cpp`:
```cpp
#include "mctest.h"
#include <string.h>
#include <vector>

struct Entry { const char *name; McTestFn fn; };

static std::vector<Entry> &Registry(void)
{
	static std::vector<Entry> r;
	return r;
}

void McTestRegister(const char *name, McTestFn fn)
{
	Entry e = { name, fn };
	Registry().push_back(e);
}

int main(int argc, char **argv)
{
	const char *filter = argc > 1 ? argv[1] : nullptr;
	int ran = 0;
	for(size_t i = 0; i < Registry().size(); i++){
		const Entry &e = Registry()[i];
		if(filter && !strstr(e.name, filter))
			continue;
		printf("[ RUN ] %s\n", e.name);
		e.fn();
		printf("[ OK  ] %s\n", e.name);
		ran++;
	}
	if(ran == 0){
		fprintf(stderr, "no tests matched\n");
		return 1;
	}
	printf("%d test(s) passed\n", ran);
	return 0;
}
```

`tests/minecraft/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.14)
project(mctests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB MC_CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../../src/minecraft/core/*.cpp)
file(GLOB MC_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(mctests ${MC_CORE_SOURCES} ${MC_TEST_SOURCES})
target_include_directories(mctests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/minecraft/core)
target_compile_options(mctests PRIVATE -Wall -Wextra)
```

- [ ] **Step 2: Write the failing tests**

`tests/minecraft/test_world.cpp`:
```cpp
#include "mctest.h"
#include "McWorld.h"

using namespace Mc;

MC_TEST(world_floor_div_mod_negative)
{
	MC_CHECK_EQ(FloorDiv(0, 16), 0);
	MC_CHECK_EQ(FloorDiv(15, 16), 0);
	MC_CHECK_EQ(FloorDiv(16, 16), 1);
	MC_CHECK_EQ(FloorDiv(-1, 16), -1);
	MC_CHECK_EQ(FloorDiv(-16, 16), -1);
	MC_CHECK_EQ(FloorDiv(-17, 16), -2);
	MC_CHECK_EQ(FloorMod(-1, 16), 15);
	MC_CHECK_EQ(FloorMod(-16, 16), 0);
	MC_CHECK_EQ(FloorMod(17, 16), 1);
}

MC_TEST(world_empty_is_air)
{
	World w;
	MC_CHECK_EQ(w.Get(0, 0, 0), BLOCK_AIR);
	MC_CHECK_EQ(w.Get(-500, 200, -3), BLOCK_AIR);
	MC_CHECK_EQ(w.ChunkCount(), 0);
}

MC_TEST(world_set_get_roundtrip_including_negative)
{
	World w;
	MC_CHECK(w.Set(3, 4, 5, BLOCK_STONE));
	MC_CHECK(w.Set(-1, -1, -1, BLOCK_DIRT));
	MC_CHECK(w.Set(-17, 40, -33, BLOCK_WOOD));
	MC_CHECK_EQ(w.Get(3, 4, 5), BLOCK_STONE);
	MC_CHECK_EQ(w.Get(-1, -1, -1), BLOCK_DIRT);
	MC_CHECK_EQ(w.Get(-17, 40, -33), BLOCK_WOOD);
	MC_CHECK_EQ(w.Get(2, 4, 5), BLOCK_AIR);
	MC_CHECK_EQ(w.ChunkCount(), 3);
}

MC_TEST(world_set_same_value_reports_no_change)
{
	World w;
	MC_CHECK(w.Set(1, 1, 1, BLOCK_DIRT));
	MC_CHECK(!w.Set(1, 1, 1, BLOCK_DIRT));
}

MC_TEST(world_set_air_in_missing_chunk_allocates_nothing)
{
	World w;
	MC_CHECK(!w.Set(100, 100, 100, BLOCK_AIR));
	MC_CHECK_EQ(w.ChunkCount(), 0);
}

MC_TEST(world_empty_chunk_is_freed)
{
	World w;
	w.Set(1, 1, 1, BLOCK_DIRT);
	w.Set(2, 1, 1, BLOCK_DIRT);
	MC_CHECK_EQ(w.ChunkCount(), 1);
	w.Set(1, 1, 1, BLOCK_AIR);
	MC_CHECK_EQ(w.ChunkCount(), 1);
	w.Set(2, 1, 1, BLOCK_AIR);
	MC_CHECK_EQ(w.ChunkCount(), 0);
}

MC_TEST(world_set_marks_chunk_and_border_neighbours_dirty)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);      // chunk (0,0,0)
	w.Set(-1, 0, 0, BLOCK_DIRT);     // chunk (-1,0,0), neighbour across the x=0 border
	ChunkPos a = { 0, 0, 0 };
	ChunkPos b = { -1, 0, 0 };
	w.FindChunk(a)->dirty = false;
	w.FindChunk(b)->dirty = false;
	w.Set(0, 5, 5, BLOCK_STONE);     // on the x=0 border of chunk a
	MC_CHECK(w.FindChunk(a)->dirty);
	MC_CHECK(w.FindChunk(b)->dirty);
}

MC_TEST(world_set_interior_does_not_dirty_neighbours)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);
	w.Set(-1, 0, 0, BLOCK_DIRT);
	ChunkPos b = { -1, 0, 0 };
	w.FindChunk(b)->dirty = false;
	w.Set(8, 8, 8, BLOCK_STONE);
	MC_CHECK(!w.FindChunk(b)->dirty);
}

MC_TEST(world_unknown_block_id_is_air_info)
{
	MC_CHECK_EQ(GetBlockInfo(200).transparent, true);
	MC_CHECK(GetBlockInfo(BLOCK_GLASS).transparent);
	MC_CHECK(!GetBlockInfo(BLOCK_STONE).transparent);
}
```

- [ ] **Step 3: Run to verify it fails**

Run: `cmake -S tests/minecraft -B build/mctest -DCMAKE_BUILD_TYPE=Debug && cmake --build build/mctest -j`
Expected: FAIL at compile, `McWorld.h: No such file or directory`.

- [ ] **Step 4: Implement**

`src/minecraft/core/McBlocks.h`:
```cpp
#pragma once
#include <stdint.h>

namespace Mc {

enum BlockId : uint8_t {
	BLOCK_AIR = 0,
	BLOCK_DIRT,
	BLOCK_STONE,
	BLOCK_WOOD,
	BLOCK_GLASS,
	BLOCK_COUNT
};

struct BlockInfo {
	const char *name;
	uint8_t r, g, b;
	bool transparent;	// faces next to it stay visible; air is transparent
};

// Unknown ids return the air entry.
const BlockInfo &GetBlockInfo(uint8_t id);

}
```

`src/minecraft/core/McBlocks.cpp`:
```cpp
#include "McBlocks.h"

namespace Mc {

static const BlockInfo blockInfo[BLOCK_COUNT] = {
	{ "air",   0,   0,   0,   true  },
	{ "dirt",  134, 96,  67,  false },
	{ "stone", 125, 125, 125, false },
	{ "wood",  160, 130, 80,  false },
	{ "glass", 190, 225, 235, true  },
};

const BlockInfo &GetBlockInfo(uint8_t id)
{
	if(id >= BLOCK_COUNT)
		return blockInfo[BLOCK_AIR];
	return blockInfo[id];
}

}
```

`src/minecraft/core/McWorld.h`:
```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <unordered_map>
#include "McBlocks.h"

namespace Mc {

const int CHUNK_SIZE = 16;
const int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

inline int ChunkIndex(int lx, int ly, int lz) { return lx + CHUNK_SIZE * (ly + CHUNK_SIZE * lz); }

// floor semantics for negative a; b must be > 0
int FloorDiv(int a, int b);
int FloorMod(int a, int b);

struct ChunkPos {
	int32_t x, y, z;
	bool operator==(const ChunkPos &o) const { return x == o.x && y == o.y && z == o.z; }
};

struct ChunkPosHash {
	size_t operator()(const ChunkPos &p) const
	{
		return (size_t)((uint32_t)p.x * 73856093u ^ (uint32_t)p.y * 19349663u ^ (uint32_t)p.z * 83492791u);
	}
};

struct Chunk {
	uint8_t blocks[CHUNK_VOLUME];
	int count;	// non-air blocks
	bool dirty;	// needs re-meshing
};

class World {
public:
	typedef std::unordered_map<ChunkPos, Chunk*, ChunkPosHash> ChunkMap;

	World() {}
	~World() { Clear(); }

	uint8_t Get(int x, int y, int z) const;
	// Returns true if the block changed. Setting air in a missing chunk is a no-op.
	bool Set(int x, int y, int z, uint8_t id);

	size_t ChunkCount() const { return m_chunks.size(); }
	Chunk *FindChunk(ChunkPos p);
	const Chunk *FindChunk(ChunkPos p) const;
	const ChunkMap &Chunks() const { return m_chunks; }
	void Clear();

	bool Save(const char *path) const;	// Task 2
	bool Load(const char *path);		// Task 2

private:
	World(const World&);
	World &operator=(const World&);
	void MarkNeighbourDirty(int cx, int cy, int cz);

	ChunkMap m_chunks;
};

}
```

`src/minecraft/core/McWorld.cpp`:
```cpp
#include "McWorld.h"
#include <string.h>

namespace Mc {

int FloorDiv(int a, int b)
{
	int q = a / b;
	if((a % b != 0) && ((a < 0) != (b < 0)))
		q--;
	return q;
}

int FloorMod(int a, int b)
{
	return a - FloorDiv(a, b) * b;
}

Chunk *World::FindChunk(ChunkPos p)
{
	ChunkMap::iterator it = m_chunks.find(p);
	return it == m_chunks.end() ? nullptr : it->second;
}

const Chunk *World::FindChunk(ChunkPos p) const
{
	ChunkMap::const_iterator it = m_chunks.find(p);
	return it == m_chunks.end() ? nullptr : it->second;
}

uint8_t World::Get(int x, int y, int z) const
{
	ChunkPos p = { FloorDiv(x, CHUNK_SIZE), FloorDiv(y, CHUNK_SIZE), FloorDiv(z, CHUNK_SIZE) };
	const Chunk *c = FindChunk(p);
	if(c == nullptr)
		return BLOCK_AIR;
	return c->blocks[ChunkIndex(FloorMod(x, CHUNK_SIZE), FloorMod(y, CHUNK_SIZE), FloorMod(z, CHUNK_SIZE))];
}

void World::MarkNeighbourDirty(int cx, int cy, int cz)
{
	ChunkPos p = { cx, cy, cz };
	Chunk *c = FindChunk(p);
	if(c)
		c->dirty = true;
}

bool World::Set(int x, int y, int z, uint8_t id)
{
	ChunkPos p = { FloorDiv(x, CHUNK_SIZE), FloorDiv(y, CHUNK_SIZE), FloorDiv(z, CHUNK_SIZE) };
	int lx = FloorMod(x, CHUNK_SIZE), ly = FloorMod(y, CHUNK_SIZE), lz = FloorMod(z, CHUNK_SIZE);
	Chunk *c = FindChunk(p);
	if(c == nullptr){
		if(id == BLOCK_AIR)
			return false;
		c = new Chunk;
		memset(c->blocks, 0, sizeof(c->blocks));
		c->count = 0;
		c->dirty = true;
		m_chunks[p] = c;
	}
	uint8_t &cell = c->blocks[ChunkIndex(lx, ly, lz)];
	if(cell == id)
		return false;
	if(cell == BLOCK_AIR)
		c->count++;
	if(id == BLOCK_AIR)
		c->count--;
	cell = id;
	c->dirty = true;

	if(lx == 0) MarkNeighbourDirty(p.x - 1, p.y, p.z);
	if(lx == CHUNK_SIZE - 1) MarkNeighbourDirty(p.x + 1, p.y, p.z);
	if(ly == 0) MarkNeighbourDirty(p.x, p.y - 1, p.z);
	if(ly == CHUNK_SIZE - 1) MarkNeighbourDirty(p.x, p.y + 1, p.z);
	if(lz == 0) MarkNeighbourDirty(p.x, p.y, p.z - 1);
	if(lz == CHUNK_SIZE - 1) MarkNeighbourDirty(p.x, p.y, p.z + 1);

	if(c->count == 0){
		delete c;
		m_chunks.erase(p);
	}
	return true;
}

void World::Clear()
{
	for(ChunkMap::iterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
		delete it->second;
	m_chunks.clear();
}

// Task 2 replaces these stubs.
bool World::Save(const char *) const { return false; }
bool World::Load(const char *) { return false; }

}
```

- [ ] **Step 5: Run to verify it passes**

Run: `cmake --build build/mctest -j && build/mctest/mctests`
Expected: `9 test(s) passed`.

- [ ] **Step 6: Commit (only if the user approved committing)**

```bash
git add tests/minecraft src/minecraft/core
git commit -m "feat(minecraft): voxel world core with tests"
```

---

### Task 2: McWorld save/load

**Files:**
- Modify: `src/minecraft/core/McWorld.cpp` (replace the two stubs)
- Test: `tests/minecraft/test_world.cpp` (append)

**Interfaces:**
- Consumes: `World`, `Chunk`, `ChunkPos` from Task 1.
- Produces: `bool World::Save(const char *path) const`, `bool World::Load(const char *path)`. File layout: bytes `'M','C','W','1'`, `uint32_t chunkCount`, then per chunk `int32_t x, y, z` and 4096 block bytes (host endianness). `Load` returns false and leaves the world **empty** on missing file, wrong magic, truncation, or a chunk count above 1,000,000. Loaded chunks are marked dirty and recomputed `count`; chunks whose blocks are all air are skipped.

- [ ] **Step 1: Write the failing tests** (append to `tests/minecraft/test_world.cpp`)

```cpp
#include <stdio.h>
#include <string.h>

static const char *kTmp = "mctest_world.tmp";

MC_TEST(world_save_load_roundtrip)
{
	World a;
	a.Set(3, 4, 5, BLOCK_STONE);
	a.Set(-40, 7, -1, BLOCK_GLASS);
	MC_CHECK(a.Save(kTmp));

	World b;
	b.Set(99, 99, 99, BLOCK_DIRT);	// must be discarded by Load
	MC_CHECK(b.Load(kTmp));
	MC_CHECK_EQ(b.Get(3, 4, 5), BLOCK_STONE);
	MC_CHECK_EQ(b.Get(-40, 7, -1), BLOCK_GLASS);
	MC_CHECK_EQ(b.Get(99, 99, 99), BLOCK_AIR);
	MC_CHECK_EQ(b.ChunkCount(), 2);
	ChunkPos p = { 0, 0, 0 };
	MC_CHECK(b.FindChunk(p)->dirty);
	MC_CHECK_EQ(b.FindChunk(p)->count, 1);
	remove(kTmp);
}

MC_TEST(world_load_missing_file_fails_and_empties)
{
	World w;
	w.Set(1, 1, 1, BLOCK_DIRT);
	MC_CHECK(!w.Load("definitely_not_here.dat"));
	MC_CHECK_EQ(w.ChunkCount(), 0);
}

MC_TEST(world_load_wrong_magic_fails)
{
	FILE *f = fopen(kTmp, "wb");
	fwrite("XXXX\0\0\0\0", 1, 8, f);
	fclose(f);
	World w;
	MC_CHECK(!w.Load(kTmp));
	MC_CHECK_EQ(w.ChunkCount(), 0);
	remove(kTmp);
}

MC_TEST(world_load_truncated_fails_and_empties)
{
	World a;
	a.Set(1, 1, 1, BLOCK_DIRT);
	MC_CHECK(a.Save(kTmp));
	// chop the file in the middle of the chunk payload
	FILE *f = fopen(kTmp, "rb");
	char buf[64];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	MC_CHECK(n == sizeof(buf));
	f = fopen(kTmp, "wb");
	fwrite(buf, 1, sizeof(buf), f);
	fclose(f);

	World w;
	MC_CHECK(!w.Load(kTmp));
	MC_CHECK_EQ(w.ChunkCount(), 0);
	remove(kTmp);
}

MC_TEST(world_load_absurd_chunk_count_fails)
{
	FILE *f = fopen(kTmp, "wb");
	fwrite("MCW1", 1, 4, f);
	uint32_t n = 0xFFFFFFFFu;
	fwrite(&n, 4, 1, f);
	fclose(f);
	World w;
	MC_CHECK(!w.Load(kTmp));
	MC_CHECK_EQ(w.ChunkCount(), 0);
	remove(kTmp);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build/mctest -j && build/mctest/mctests world_save`
Expected: FAIL (`CHECK failed: a.Save(kTmp)`, since the stub returns false).

- [ ] **Step 3: Implement** (in `McWorld.cpp`, replace the stubs and add `#include <stdio.h>`)

```cpp
bool World::Save(const char *path) const
{
	FILE *f = fopen(path, "wb");
	if(f == nullptr)
		return false;
	uint32_t n = (uint32_t)m_chunks.size();
	bool ok = fwrite("MCW1", 1, 4, f) == 4 && fwrite(&n, sizeof(n), 1, f) == 1;
	for(ChunkMap::const_iterator it = m_chunks.begin(); ok && it != m_chunks.end(); ++it){
		int32_t pos[3] = { it->first.x, it->first.y, it->first.z };
		ok = fwrite(pos, sizeof(pos), 1, f) == 1 &&
			fwrite(it->second->blocks, 1, CHUNK_VOLUME, f) == (size_t)CHUNK_VOLUME;
	}
	if(fclose(f) != 0)
		ok = false;
	return ok;
}

bool World::Load(const char *path)
{
	Clear();
	FILE *f = fopen(path, "rb");
	if(f == nullptr)
		return false;
	char magic[4];
	uint32_t n = 0;
	bool ok = fread(magic, 1, 4, f) == 4 && memcmp(magic, "MCW1", 4) == 0 &&
		fread(&n, sizeof(n), 1, f) == 1 && n <= 1000000u;
	for(uint32_t i = 0; ok && i < n; i++){
		int32_t pos[3];
		Chunk *c = new Chunk;
		ok = fread(pos, sizeof(pos), 1, f) == 1 &&
			fread(c->blocks, 1, CHUNK_VOLUME, f) == (size_t)CHUNK_VOLUME;
		if(!ok){
			delete c;
			break;
		}
		c->count = 0;
		for(int j = 0; j < CHUNK_VOLUME; j++)
			if(c->blocks[j] != BLOCK_AIR)
				c->count++;
		c->dirty = true;
		ChunkPos p = { pos[0], pos[1], pos[2] };
		ChunkMap::iterator old = m_chunks.find(p);
		if(c->count == 0){
			delete c;
		}else{
			if(old != m_chunks.end())
				delete old->second;
			m_chunks[p] = c;
		}
	}
	fclose(f);
	if(!ok)
		Clear();
	return ok;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build/mctest -j && build/mctest/mctests`
Expected: `14 test(s) passed`.

- [ ] **Step 5: Commit (only if approved)**

```bash
git add tests/minecraft src/minecraft/core/McWorld.cpp
git commit -m "feat(minecraft): save and load voxel world"
```

---

### Task 3: McRay (voxel DDA raycast)

**Files:**
- Create: `src/minecraft/core/McRay.h`, `src/minecraft/core/McRay.cpp`, `tests/minecraft/test_ray.cpp`

**Interfaces:**
- Consumes: `World::Get`, `BLOCK_AIR`.
- Produces: `struct RayHit { bool hit; int x, y, z; int nx, ny, nz; float t; }` and `RayHit RayCast(const World &w, float ox, float oy, float oz, float dx, float dy, float dz, float maxDist)`. `(x,y,z)` is the solid cell hit; `(nx,ny,nz)` is the unit normal of the face entered, pointing back toward the ray origin (so the cell to place a block in is `x+nx, y+ny, z+nz`); `t` is the distance along the (unit) direction. If the origin is inside a solid cell the hit is `t = 0` with normal `(0,0,0)`. The direction need not be normalised; `t` is then in units of the direction vector. `maxDist` is in the same units as `t`.

- [ ] **Step 1: Write the failing tests**

`tests/minecraft/test_ray.cpp`:
```cpp
#include "mctest.h"
#include "McRay.h"

using namespace Mc;

MC_TEST(ray_hits_block_along_plus_x)
{
	World w;
	w.Set(3, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, 3); MC_CHECK_EQ(h.y, 0); MC_CHECK_EQ(h.z, 0);
	MC_CHECK_EQ(h.nx, -1); MC_CHECK_EQ(h.ny, 0); MC_CHECK_EQ(h.nz, 0);
	MC_CHECK_NEAR(h.t, 2.5, 1e-4);
}

MC_TEST(ray_hits_block_along_minus_x_negative_cells)
{
	World w;
	w.Set(-2, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, -1, 0, 0, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, -2);
	MC_CHECK_EQ(h.nx, 1);
	MC_CHECK_NEAR(h.t, 1.5, 1e-4);
}

MC_TEST(ray_hits_top_face_going_down)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);
	RayHit h = RayCast(w, 0.5f, 0.5f, 5.5f, 0, 0, -1, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.nz, 1);
	MC_CHECK_NEAR(h.t, 4.5, 1e-4);
}

MC_TEST(ray_hits_block_in_negative_chunk)
{
	World w;
	w.Set(-20, -20, -20, BLOCK_WOOD);
	RayHit h = RayCast(w, -10.5f, -19.5f, -19.5f, -1, 0, 0, 20);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, -20);
	MC_CHECK_EQ(h.nx, 1);
}

MC_TEST(ray_misses_beyond_max_distance)
{
	World w;
	w.Set(10, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 5);
	MC_CHECK(!h.hit);
}

MC_TEST(ray_misses_in_empty_world)
{
	World w;
	MC_CHECK(!RayCast(w, 0, 0, 0, 0.3f, 0.4f, 0.5f, 100).hit);
}

MC_TEST(ray_zero_direction_misses)
{
	World w;
	w.Set(1, 0, 0, BLOCK_STONE);
	MC_CHECK(!RayCast(w, 0.5f, 0.5f, 0.5f, 0, 0, 0, 10).hit);
}

MC_TEST(ray_starting_inside_block_hits_with_zero_normal)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.nx, 0); MC_CHECK_EQ(h.ny, 0); MC_CHECK_EQ(h.nz, 0);
	MC_CHECK_NEAR(h.t, 0, 1e-6);
}

MC_TEST(ray_returns_nearest_block)
{
	World w;
	w.Set(2, 0, 0, BLOCK_STONE);
	w.Set(5, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK_EQ(h.x, 2);
}

MC_TEST(ray_diagonal_hits_expected_cell)
{
	World w;
	w.Set(3, 3, 0, BLOCK_STONE);
	// from the centre of (0,0) toward the centre of (3,3): passes through the cell corners
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 1, 0, 20);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, 3); MC_CHECK_EQ(h.y, 3);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j`
Expected: FAIL at compile, `McRay.h: No such file or directory`.

- [ ] **Step 3: Implement**

`src/minecraft/core/McRay.h`:
```cpp
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
```

`src/minecraft/core/McRay.cpp`:
```cpp
#include "McRay.h"
#include <math.h>

namespace Mc {

RayHit RayCast(const World &w, float ox, float oy, float oz, float dx, float dy, float dz, float maxDist)
{
	RayHit miss = { false, 0, 0, 0, 0, 0, 0, 0.0f };
	const float o[3] = { ox, oy, oz };
	const float d[3] = { dx, dy, dz };
	int cell[3], step[3];
	float tMax[3], tDelta[3];
	const float inf = 1e30f;

	for(int a = 0; a < 3; a++){
		cell[a] = (int)floorf(o[a]);
		if(d[a] > 0.0f){
			step[a] = 1;
			tDelta[a] = 1.0f / d[a];
			tMax[a] = ((float)(cell[a] + 1) - o[a]) / d[a];
		}else if(d[a] < 0.0f){
			step[a] = -1;
			tDelta[a] = -1.0f / d[a];
			tMax[a] = (o[a] - (float)cell[a]) / -d[a];
		}else{
			step[a] = 0;
			tDelta[a] = inf;
			tMax[a] = inf;
		}
	}

	float t = 0.0f;
	int n[3] = { 0, 0, 0 };
	for(;;){
		if(w.Get(cell[0], cell[1], cell[2]) != BLOCK_AIR){
			RayHit h = { true, cell[0], cell[1], cell[2], n[0], n[1], n[2], t };
			return h;
		}
		int a;
		if(tMax[0] < tMax[1])
			a = tMax[0] < tMax[2] ? 0 : 2;
		else
			a = tMax[1] < tMax[2] ? 1 : 2;
		if(tMax[a] > maxDist)
			return miss;
		t = tMax[a];
		cell[a] += step[a];
		tMax[a] += tDelta[a];
		n[0] = n[1] = n[2] = 0;
		n[a] = -step[a];
	}
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build/mctest -j && build/mctest/mctests ray_`
Expected: `10 test(s) passed`.

- [ ] **Step 5: Commit (only if approved)**

```bash
git add tests/minecraft src/minecraft/core/McRay.*
git commit -m "feat(minecraft): voxel DDA raycast"
```

---

### Task 4: McMesher

**Files:**
- Create: `src/minecraft/core/McMesher.h`, `src/minecraft/core/McMesher.cpp`, `tests/minecraft/test_mesher.cpp`

**Interfaces:**
- Consumes: `World`, `Chunk`, `ChunkPos`, `GetBlockInfo`, `ChunkIndex`.
- Produces:
  - `struct McVertex { float x, y, z; float u, v; uint8_t r, g, b, a; }` (absolute world coordinates, so no per-chunk transform is needed)
  - `struct ChunkMesh { std::vector<McVertex> verts; std::vector<uint16_t> idx; }`
  - `void MeshChunk(const World &w, ChunkPos cp, bool textured, ChunkMesh &out)`: clears `out`, then emits one quad (4 verts, 6 indices, counter-clockwise seen from outside) per visible face. A face is visible if the neighbouring cell is air, or is transparent and of a different id. `textured == false`: vertex colour = block colour x face shade. `textured == true`: vertex colour = white x face shade, and `u,v` address a 4x4 atlas tile `(id % 4, id / 4)`. Face shade: top (+Z) 1.0, bottom (-Z) 0.5, +/-Y 0.8, +/-X 0.6. `a` is 255 for solid blocks and 140 for transparent ones.

- [ ] **Step 1: Write the failing tests**

`tests/minecraft/test_mesher.cpp`:
```cpp
#include "mctest.h"
#include "McMesher.h"

using namespace Mc;

static ChunkPos origin() { ChunkPos p = { 0, 0, 0 }; return p; }

MC_TEST(mesh_empty_chunk_is_empty)
{
	World w;
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 0);
	MC_CHECK_EQ(m.idx.size(), 0);
}

MC_TEST(mesh_single_block_has_six_faces)
{
	World w;
	w.Set(5, 5, 5, BLOCK_STONE);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 24);
	MC_CHECK_EQ(m.idx.size(), 36);
}

MC_TEST(mesh_two_adjacent_blocks_hide_shared_faces)
{
	World w;
	w.Set(5, 5, 5, BLOCK_STONE);
	w.Set(6, 5, 5, BLOCK_STONE);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 10 * 4);
}

MC_TEST(mesh_culls_against_block_in_neighbouring_chunk)
{
	World w;
	w.Set(15, 0, 0, BLOCK_STONE);   // chunk (0,0,0), east border
	w.Set(16, 0, 0, BLOCK_STONE);   // chunk (1,0,0)
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 5 * 4);
}

MC_TEST(mesh_glass_next_to_stone_keeps_stone_face_but_not_glass_face)
{
	World w;
	w.Set(5, 5, 5, BLOCK_STONE);
	w.Set(6, 5, 5, BLOCK_GLASS);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	// stone: 6 faces (glass is transparent, so the shared face stays); glass: 5 (stone hides one)
	MC_CHECK_EQ(m.verts.size(), 11 * 4);
}

MC_TEST(mesh_glass_next_to_glass_hides_shared_faces)
{
	World w;
	w.Set(5, 5, 5, BLOCK_GLASS);
	w.Set(6, 5, 5, BLOCK_GLASS);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 10 * 4);
}

MC_TEST(mesh_quads_face_outward_with_ccw_winding)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.idx.size(), 36);
	for(size_t t = 0; t + 2 < m.idx.size(); t += 3){
		const McVertex &a = m.verts[m.idx[t]], &b = m.verts[m.idx[t + 1]], &c = m.verts[m.idx[t + 2]];
		float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
		float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
		float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
		// centre of the block is (0.5,0.5,0.5); the face normal must point away from it
		float cx = (a.x + b.x + c.x) / 3 - 0.5f, cy = (a.y + b.y + c.y) / 3 - 0.5f, cz = (a.z + b.z + c.z) / 3 - 0.5f;
		MC_CHECK(nx * cx + ny * cy + nz * cz > 0.0f);
	}
}

MC_TEST(mesh_uses_absolute_world_coordinates_including_negative_chunks)
{
	World w;
	w.Set(-1, -1, -1, BLOCK_DIRT);
	ChunkMesh m;
	ChunkPos p = { -1, -1, -1 };
	MeshChunk(w, p, false, m);
	MC_CHECK_EQ(m.verts.size(), 24);
	float minx = 1e9f, maxx = -1e9f;
	for(size_t i = 0; i < m.verts.size(); i++){
		if(m.verts[i].x < minx) minx = m.verts[i].x;
		if(m.verts[i].x > maxx) maxx = m.verts[i].x;
	}
	MC_CHECK_NEAR(minx, -1.0, 1e-5);
	MC_CHECK_NEAR(maxx, 0.0, 1e-5);
}

MC_TEST(mesh_flat_colour_vs_textured)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	ChunkMesh flat, tex;
	MeshChunk(w, origin(), false, flat);
	MeshChunk(w, origin(), true, tex);
	// find a top-face vertex (z == 1 for all four corners of the face)
	bool found = false;
	for(size_t i = 0; i < flat.verts.size(); i++){
		if(flat.verts[i].z == 1.0f){
			MC_CHECK_EQ(flat.verts[i].r, 125);   // stone colour x shade 1.0
			MC_CHECK_EQ(tex.verts[i].r, 255);    // white x shade 1.0
			found = true;
			break;
		}
	}
	MC_CHECK(found);
}

MC_TEST(mesh_worst_case_checkerboard_fits_16_bit_indices)
{
	World w;
	for(int z = 0; z < 16; z++)
		for(int y = 0; y < 16; y++)
			for(int x = 0; x < 16; x++)
				if((x + y + z) % 2 == 0)
					w.Set(x, y, z, BLOCK_STONE);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK(m.verts.size() <= 65535);
	MC_CHECK(m.verts.size() > 40000);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j`
Expected: FAIL at compile, `McMesher.h: No such file or directory`.

- [ ] **Step 3: Implement**

`src/minecraft/core/McMesher.h`:
```cpp
#pragma once
#include <vector>
#include "McWorld.h"

namespace Mc {

struct McVertex {
	float x, y, z;
	float u, v;
	uint8_t r, g, b, a;
};

struct ChunkMesh {
	std::vector<McVertex> verts;
	std::vector<uint16_t> idx;
};

void MeshChunk(const World &w, ChunkPos cp, bool textured, ChunkMesh &out);

}
```

`src/minecraft/core/McMesher.cpp`:
```cpp
#include "McMesher.h"

namespace Mc {

// face = 0..5: +X, -X, +Y, -Y, +Z, -Z
static const float faceShade[3][2] = {
	{ 0.6f, 0.6f },	// X
	{ 0.8f, 0.8f },	// Y
	{ 1.0f, 0.5f },	// Z (top, bottom)
};

void MeshChunk(const World &w, ChunkPos cp, bool textured, ChunkMesh &out)
{
	out.verts.clear();
	out.idx.clear();
	const Chunk *c = w.FindChunk(cp);
	if(c == nullptr)
		return;

	for(int lz = 0; lz < CHUNK_SIZE; lz++)
	for(int ly = 0; ly < CHUNK_SIZE; ly++)
	for(int lx = 0; lx < CHUNK_SIZE; lx++){
		uint8_t id = c->blocks[ChunkIndex(lx, ly, lz)];
		if(id == BLOCK_AIR)
			continue;
		const BlockInfo &info = GetBlockInfo(id);
		int pos[3] = { cp.x * CHUNK_SIZE + lx, cp.y * CHUNK_SIZE + ly, cp.z * CHUNK_SIZE + lz };

		for(int face = 0; face < 6; face++){
			int a = face / 2;		// axis
			int s = (face % 2) ? -1 : 1;	// sign
			int nb[3] = { pos[0], pos[1], pos[2] };
			nb[a] += s;
			uint8_t nid = w.Get(nb[0], nb[1], nb[2]);
			if(nid != BLOCK_AIR && !(GetBlockInfo(nid).transparent && nid != id))
				continue;

			float shade = faceShade[a][face % 2];
			uint8_t r, g, b;
			if(textured){
				r = g = b = (uint8_t)(255.0f * shade);
			}else{
				r = (uint8_t)(info.r * shade);
				g = (uint8_t)(info.g * shade);
				b = (uint8_t)(info.b * shade);
			}
			uint8_t alpha = info.transparent ? 140 : 255;

			int ua = (a + 1) % 3, va = (a + 2) % 3;	// (a, ua, va) is cyclic, so ua x va = +a
			static const int cu[4] = { 0, 1, 1, 0 };
			static const int cv[4] = { 0, 0, 1, 1 };
			int order[4] = { 0, 1, 2, 3 };
			if(s < 0){
				order[1] = 3;
				order[3] = 1;	// reverse winding for negative faces
			}
			uint16_t base = (uint16_t)out.verts.size();
			for(int k = 0; k < 4; k++){
				int i = order[k];
				float p[3];
				p[a] = (float)(pos[a] + (s > 0 ? 1 : 0));
				p[ua] = (float)(pos[ua] + cu[i]);
				p[va] = (float)(pos[va] + cv[i]);

				float tu, tv;
				if(a == 2){ tu = (float)cu[i]; tv = (float)cv[i]; }
				else if(a == 0){ tu = (float)cu[i]; tv = 1.0f - cv[i]; }
				else { tu = (float)cv[i]; tv = 1.0f - cu[i]; }
				float tx = (float)(id % 4), ty = (float)(id / 4);

				McVertex v;
				v.x = p[0]; v.y = p[1]; v.z = p[2];
				v.u = (tx + tu) / 4.0f;
				v.v = (ty + tv) / 4.0f;
				v.r = r; v.g = g; v.b = b; v.a = alpha;
				out.verts.push_back(v);
			}
			static const uint16_t quad[6] = { 0, 1, 2, 0, 2, 3 };
			for(int k = 0; k < 6; k++)
				out.idx.push_back((uint16_t)(base + quad[k]));
		}
	}
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build/mctest -j && build/mctest/mctests`
Expected: `34 test(s) passed` (9 + 5 + 10 + 10). If `mesh_quads_face_outward_with_ccw_winding` fails, the `order[]` swap for negative faces is wrong; fix it here, do not loosen the test.

- [ ] **Step 5: Commit (only if approved)**

```bash
git add tests/minecraft src/minecraft/core/McMesher.*
git commit -m "feat(minecraft): chunk mesher with face culling"
```

---

### Task 5: MINECRAFT_MODE flag, McMode skeleton and the four hooks

**Files:**
- Create: `src/minecraft/game/McMode.h`, `src/minecraft/game/McMode.cpp`
- Modify: `src/core/config.h` (one block), `src/core/Game.cpp` (3 hook lines + include), `src/core/main.cpp` (1 hook line + include)

**Interfaces:**
- Consumes: `Mc::World`.
- Produces (namespace `McMode`):
  - `void Init(void)`: loads `mcworld.dat` if present (a failed load leaves the world empty and logs)
  - `void Shutdown(void)`: saves `mcworld.dat` (only if the world has chunks, so an untouched session does not create a file)
  - `void Update(void)`: handles the F8 toggle; later tasks add logic here
  - `void Render(void)`: empty until Task 6
  - `bool IsActive(void)`
  - `Mc::World &GetWorld(void)`

The game-layer files compile only with `MINECRAFT_MODE`; wrap the whole `.cpp` body in `#ifdef MINECRAFT_MODE`, and `McMode.h` declares the functions unconditionally but hooks call them only under the flag.

- [ ] **Step 1: Add the flag to `src/core/config.h`**

Find the `#define NEW_RENDERER          // leeds-like world rendering, needs librw` line (around line 317) and add directly after it:
```cpp
#define MINECRAFT_MODE        // Steve mode: voxel blocks in the GTA world (src/minecraft)
```
Do not touch anything else in the file.

- [ ] **Step 2: Create `src/minecraft/game/McMode.h`**

```cpp
#pragma once

#include "McWorld.h"

namespace McMode
{
	void Init(void);
	void Shutdown(void);
	void Update(void);
	void Render(void);
	bool IsActive(void);
	Mc::World &GetWorld(void);
}
```

- [ ] **Step 3: Create `src/minecraft/game/McMode.cpp`**

```cpp
#include "common.h"

#ifdef MINECRAFT_MODE

#include "Pad.h"
#include "McMode.h"

static const char *saveFile = "mcworld.dat";
static const int toggleKey = 7;	// F8, zero based

static Mc::World world;
static bool active;

namespace McMode
{

Mc::World &
GetWorld(void)
{
	return world;
}

bool
IsActive(void)
{
	return active;
}

void
Init(void)
{
	active = false;
	if(world.Load(saveFile))
		printf("McMode: loaded %s (%d chunks)\n", saveFile, (int)world.ChunkCount());
}

void
Shutdown(void)
{
	if(world.ChunkCount() != 0)
		if(!world.Save(saveFile))
			printf("McMode: failed to save %s\n", saveFile);
	world.Clear();
	active = false;
}

void
Update(void)
{
	if(CPad::GetPad(0)->GetFJustDown(toggleKey)){
		active = !active;
		printf("McMode: Steve mode %s\n", active ? "ON" : "OFF");
	}
}

void
Render(void)
{
}

}

#endif
```

- [ ] **Step 4: Add the hooks**

In `src/core/Game.cpp`:
1. After the line `#include "TexturePools.h"` add:
```cpp
#ifdef MINECRAFT_MODE
#include "McMode.h"
#endif
```
2. In `CGame::Initialise(const char* datFile)`, at its final `return true;` (line ~665, just before the closing `}` of that function) insert above the `return true;`:
```cpp
#ifdef MINECRAFT_MODE
	McMode::Init();
#endif
```
3. In `CGame::ShutDown(void)` (line ~668), as the first statement after the opening `{`, before `CReplay::FinishPlayback();`:
```cpp
#ifdef MINECRAFT_MODE
	McMode::Shutdown();
#endif
```
4. In `CGame::Process(void)`, directly after the line `CWorld::Process();` insert:
```cpp
#ifdef MINECRAFT_MODE
		McMode::Update();
#endif
```

In `src/core/main.cpp`:
1. After the line `#include "GitSHA1.h"` add:
```cpp
#ifdef MINECRAFT_MODE
#include "McMode.h"
#endif
```
2. In `RenderEffects(void)` (line ~1437), add as the first statement of the function body, before the `#ifdef NEW_RENDERER` block, so it runs for both renderers:
```cpp
#ifdef MINECRAFT_MODE
	McMode::Render();
#endif
```
Note: this places the call at the start of `RenderEffects`, which runs after the scene. Particles and other effects draw after blocks, which is correct for depth.

- [ ] **Step 5: Build**

Run: `cd build && cmake . && cmake --build . -j$(nproc) 2>&1 | grep -E "error|Linking|McMode" ; cd ..`
Expected: no `error` lines, ends with `Linking CXX executable src/re3`. (`cmake .` re-globs the new files.)

- [ ] **Step 6: Verify in game (manual; needs the user's GTA III path)**

Ask the user for the GTA III install path if not yet known, copy or run `build/src/re3` from that folder, start a game, press F8 and confirm the terminal prints `McMode: Steve mode ON`, press again for `OFF`. Quit and confirm no `mcworld.dat` was created.

- [ ] **Step 7: Commit (only if approved)**

```bash
git add src/minecraft/game src/core/Game.cpp src/core/main.cpp src/core/config.h
git commit -m "feat(minecraft): McMode skeleton, F8 toggle and hooks"
```
(`config.h` also carries the user's earlier uncommitted line-ending changes; ask the user before staging it.)

---

### Task 6: McRenderer (draw chunk meshes)

**Files:**
- Create: `src/minecraft/game/McRenderer.h`, `src/minecraft/game/McRenderer.cpp`
- Modify: `src/minecraft/game/McMode.cpp` (`Render` calls the renderer)

**Interfaces:**
- Consumes: `Mc::World`, `Mc::MeshChunk`, `Mc::ChunkMesh`, `Mc::McVertex`; re3's `RwIm3DVertex`, `RwIm3DVertexSetPos/U/V/RGBA`, `RwIm3DTransform`, `RwIm3DRenderIndexedPrimitive`, `RwIm3DEnd`, `RwRenderStateSet` (as used in `src/renderer/Rubbish.cpp`).
- Produces: `McRenderer::Render(const Mc::World &world)` and `McRenderer::Shutdown()`. Meshes are cached per chunk and rebuilt when `Chunk::dirty` is set (the renderer clears the flag). Meshes of chunks that no longer exist are dropped. Flat colours (`textured = false`).

- [ ] **Step 1: Create `src/minecraft/game/McRenderer.h`**

```cpp
#pragma once

#include "McWorld.h"

namespace McRenderer
{
	// Rebuilds dirty chunk meshes (clearing their dirty flag) and draws all chunks.
	void Render(Mc::World &world);
	void Shutdown(void);
}
```
(Takes a non-const `World&` because it clears `Chunk::dirty`.)

- [ ] **Step 2: Create `src/minecraft/game/McRenderer.cpp`**

```cpp
#include "common.h"

#ifdef MINECRAFT_MODE

#include "main.h"
#include "McRenderer.h"
#include "McMesher.h"
#include <unordered_map>
#include <vector>

typedef std::unordered_map<Mc::ChunkPos, Mc::ChunkMesh, Mc::ChunkPosHash> MeshMap;
static MeshMap meshes;
static std::vector<RwIm3DVertex> vertexBuffer;
static std::vector<RwImVertexIndex> indexBuffer;

namespace McRenderer
{

static void
DrawMesh(const Mc::ChunkMesh &m)
{
	if(m.idx.empty())
		return;
	vertexBuffer.resize(m.verts.size());
	for(size_t i = 0; i < m.verts.size(); i++){
		const Mc::McVertex &v = m.verts[i];
		RwIm3DVertexSetPos(&vertexBuffer[i], v.x, v.y, v.z);
		RwIm3DVertexSetU(&vertexBuffer[i], v.u);
		RwIm3DVertexSetV(&vertexBuffer[i], v.v);
		RwIm3DVertexSetRGBA(&vertexBuffer[i], v.r, v.g, v.b, v.a);
	}
	indexBuffer.assign(m.idx.begin(), m.idx.end());
	if(RwIm3DTransform(vertexBuffer.data(), vertexBuffer.size(), nil, rwIM3D_VERTEXUV)){
		RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, indexBuffer.data(), indexBuffer.size());
		RwIm3DEnd();
	}
}

void
Render(Mc::World &world)
{
	// drop meshes of freed chunks
	for(MeshMap::iterator it = meshes.begin(); it != meshes.end();){
		if(world.FindChunk(it->first) == nil)
			it = meshes.erase(it);
		else
			++it;
	}
	// rebuild dirty chunks
	for(Mc::World::ChunkMap::const_iterator it = world.Chunks().begin(); it != world.Chunks().end(); ++it){
		Mc::Chunk *c = it->second;
		if(c->dirty || meshes.find(it->first) == meshes.end()){
			Mc::MeshChunk(world, it->first, false, meshes[it->first]);
			c->dirty = false;
		}
	}
	if(meshes.empty())
		return;

	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)TRUE);

	for(MeshMap::const_iterator it = meshes.begin(); it != meshes.end(); ++it)
		DrawMesh(it->second);

	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLBACK);
}

void
Shutdown(void)
{
	meshes.clear();
}

}

#endif
```

Note: `main.h` is included for the render-state helpers used elsewhere in `src/core`. If the build reports it unnecessary or missing a symbol, check the includes at the top of `src/renderer/Rubbish.cpp` and mirror them. The vertex buffer must be a `std::vector<RwIm3DVertex>` (`RwIm3DVertex` is `rw::RWDEVICE::Im3DVertex`, default-constructible).

- [ ] **Step 3: Call it from `McMode::Render` and `McMode::Shutdown`**

In `src/minecraft/game/McMode.cpp` add `#include "McRenderer.h"` after `#include "McMode.h"`, and change:
```cpp
void
Render(void)
{
	McRenderer::Render(world);
}
```
and in `Shutdown()` add `McRenderer::Shutdown();` after `world.Clear();`.

- [ ] **Step 4: Temporary visible test block**

In `McMode::Update`, inside the F8 branch, when turning the mode ON, place a test block two units in front of the player so there is something to see. Add `#include "PlayerInfo.h"` and `#include "Ped.h"` at the top, and:
```cpp
		if(active){
			CVector p = FindPlayerCoors() + FindPlayerPed()->GetForward() * 3.0f;
			world.Set((int)floorf(p.x), (int)floorf(p.y), (int)floorf(FindPlayerCoors().z), Mc::BLOCK_STONE);
		}
```
This is throwaway: it is removed in Task 7 Step 4.

- [ ] **Step 5: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Linking"`
Expected: no `error` lines, ends with `Linking CXX executable src/re3`.

- [ ] **Step 6: Verify in game (manual)**

Run the game, start a new game, press F8. A grey 1 m cube must appear in front of the player, lit flat (not affected by GTA lighting) and occluded correctly when a car or wall passes in front. Quit and confirm `mcworld.dat` exists; relaunch, load a game, and confirm the cube is still there. If the cube is invisible, check in this order: (1) the terminal line `McMode: Steve mode ON`, (2) the `RenderEffects` hook is reached (add a temporary `printf`), (3) the render states, comparing with `Rubbish.cpp`.

- [ ] **Step 7: Commit (only if approved)**

```bash
git add src/minecraft/game
git commit -m "feat(minecraft): draw voxel chunks with RwIm3D"
```

---

### Task 7: McInteract (place, break, hotbar)

**Files:**
- Create: `src/minecraft/game/McInteract.h`, `src/minecraft/game/McInteract.cpp`
- Modify: `src/minecraft/game/McMode.cpp` (call `McInteract::Update` while active; remove the Task 6 test block)

**Interfaces:**
- Consumes: `Mc::RayCast`, `Mc::World`, `CWorld::ProcessLineOfSight`, `CColPoint`, `TheCamera`, `CPad` mouse state, `FindPlayerPed`.
- Produces: `McInteract::Update(Mc::World &world)` and, for testing the placement rule, a pure function in the core-free header: `bool McInteract::CanPlace(const Mc::RayHit &hit)` returning `hit.hit && !(hit.nx == 0 && hit.ny == 0 && hit.nz == 0)`. Behaviour: left mouse just-down breaks the aimed block; right mouse just-down places the selected block in the cell in front of the hit face; mouse wheel cycles the selected block through `BLOCK_DIRT..BLOCK_GLASS` and prints its name. The ray starts at the camera, reach is 6 units, and a block is only targeted if it is closer than any GTA building/vehicle/ped/object hit along the same ray.

- [ ] **Step 1: Verify the APIs this task relies on**

Run:
```
grep -n "CVector point" src/collision/Collision.h
grep -n "IsPed\b\|IsVehicle\b" src/entities/Entity.h
grep -n "GetLeftMouseJustDown\|GetMouseWheelUpJustDown" src/core/Pad.h
```
Expected: `CColPoint` has a `CVector point` member; `GetLeftMouseJustDown`/`GetRightMouseJustDown`/`GetMouseWheelUpJustDown`/`GetMouseWheelDownJustDown` exist (seen at `Pad.h:272-276`). If `point` is named differently, use that name below.

- [ ] **Step 2: Write `src/minecraft/game/McInteract.h`**

```cpp
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
```

- [ ] **Step 3: Add a unit test for `CanPlace` in the core test build**

`CanPlace` depends only on `McRay.h`, so test it outside the game. Append to `tests/minecraft/test_ray.cpp`:
```cpp
#include "../../src/minecraft/game/McInteract.h"

MC_TEST(interact_cannot_place_when_ray_starts_inside_block)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	RayHit inside = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK(!McInteract::CanPlace(inside));
	RayHit miss = RayCast(w, 5.5f, 5.5f, 5.5f, 1, 0, 0, 10);
	MC_CHECK(!McInteract::CanPlace(miss));
	w.Set(4, 0, 0, BLOCK_STONE);
	RayHit face = RayCast(w, 2.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK(McInteract::CanPlace(face));
}
```
(`McInteract.h` is header-only for this function; the test build's include path already contains `src/minecraft/core`, which `McInteract.h` needs for `McWorld.h` and `McRay.h`.)

Run: `cmake --build build/mctest -j && build/mctest/mctests interact_`
Expected: PASS.

- [ ] **Step 4: Write `src/minecraft/game/McInteract.cpp`**

```cpp
#include "common.h"

#ifdef MINECRAFT_MODE

#include "Pad.h"
#include "Camera.h"
#include "World.h"
#include "Collision.h"
#include "PlayerInfo.h"
#include "McInteract.h"

static const float reach = 6.0f;
static uint8_t selected = Mc::BLOCK_STONE;

namespace McInteract
{

// Distance from the camera to the nearest GTA geometry along the same ray, or reach+1 if none.
static float
GtaHitDistance(const CVector &from, const CVector &to)
{
	CColPoint colPoint;
	CEntity *entity = nil;
	if(CWorld::ProcessLineOfSight(from, to, colPoint, entity, true, true, true, true, false, true))
		return (colPoint.point - from).Magnitude();
	return reach + 1.0f;
}

void
Update(Mc::World &world)
{
	CPad *pad = CPad::GetPad(0);

	if(pad->GetMouseWheelUpJustDown() || pad->GetMouseWheelDownJustDown()){
		int n = Mc::BLOCK_COUNT - 1;	// selectable: 1..BLOCK_COUNT-1
		int i = selected - 1;
		i += pad->GetMouseWheelUpJustDown() ? 1 : -1;
		i = (i % n + n) % n;
		selected = (uint8_t)(i + 1);
		printf("McInteract: selected block %s\n", Mc::GetBlockInfo(selected).name);
	}

	bool breakBlock = pad->GetLeftMouseJustDown();
	bool placeBlock = pad->GetRightMouseJustDown();
	if(!breakBlock && !placeBlock)
		return;

	CVector origin = TheCamera.GetPosition();
	CVector dir = TheCamera.GetForward();
	dir.Normalise();
	Mc::RayHit hit = Mc::RayCast(world, origin.x, origin.y, origin.z, dir.x, dir.y, dir.z, reach);
	if(!hit.hit)
		return;
	if(GtaHitDistance(origin, origin + dir * reach) < hit.t)
		return;	// GTA geometry is in front of the block

	if(breakBlock){
		world.Set(hit.x, hit.y, hit.z, Mc::BLOCK_AIR);
	}else if(CanPlace(hit)){
		int px = hit.x + hit.nx, py = hit.y + hit.ny, pz = hit.z + hit.nz;
		// do not place inside the player's own body
		CVector feet = FindPlayerCoors();
		bool insidePlayer = px == (int)floorf(feet.x) && py == (int)floorf(feet.y) &&
			(pz == (int)floorf(feet.z) || pz == (int)floorf(feet.z + 1.0f));
		if(!insidePlayer)
			world.Set(px, py, pz, selected);
	}
}

}

#endif
```

- [ ] **Step 5: Wire it into `McMode::Update` and remove the Task 6 test block**

In `McMode.cpp`: add `#include "McInteract.h"`, delete the `if(active){ CVector p = ... world.Set(...) }` block and the `PlayerInfo.h`/`Ped.h` includes added for it in Task 6 Step 4, and at the end of `Update` add:
```cpp
	if(active)
		McInteract::Update(world);
```

- [ ] **Step 6: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Linking"`
Expected: no `error`, ends with `Linking CXX executable src/re3`.

- [ ] **Step 7: Verify in game (manual)**

F8 on, look at the ground or a wall: right click places a block on that face, left click breaks it, mouse wheel cycles dirt/stone/wood/glass (the terminal prints the name). Also check: aiming at a car door in front of a placed block must not break the block behind it; placing at your own feet must be refused.

- [ ] **Step 8: Commit (only if approved)**

```bash
git add tests/minecraft src/minecraft/game
git commit -m "feat(minecraft): place and break blocks, block cycling"
```

---

### Task 8: McCollide (AABB vs voxel push-out) and applying it to the player

**Files:**
- Create: `src/minecraft/core/McCollide.h`, `src/minecraft/core/McCollide.cpp`, `tests/minecraft/test_collide.cpp`
- Modify: `src/minecraft/game/McMode.cpp` (apply after `CWorld::Process`)

**Interfaces:**
- Consumes: `World::Get`, `BLOCK_AIR`, `GetBlockInfo`.
- Produces: `struct CollideResult { bool moved; bool onGround; }` and `CollideResult PushOutOfBlocks(const World &w, float &px, float &py, float &pz, float halfWidth, float height)`. `(px,py,pz)` is the centre of the box's feet; the box spans `[px-halfWidth, px+halfWidth] x [py-halfWidth, py+halfWidth] x [pz, pz+height]`. It repeatedly finds the overlapping solid voxel and pushes the box out along the axis of smallest penetration (overlap threshold 1e-4), up to 8 iterations. `onGround` is true if the last push was upward (+Z). Transparent blocks (glass) are solid for collision; only air is passable.

- [ ] **Step 1: Write the failing tests**

`tests/minecraft/test_collide.cpp`:
```cpp
#include "mctest.h"
#include "McCollide.h"

using namespace Mc;

MC_TEST(collide_no_overlap_changes_nothing)
{
	World w;
	w.Set(5, 5, 5, BLOCK_STONE);
	float x = 0.5f, y = 0.5f, z = 0.0f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(!r.moved);
	MC_CHECK(!r.onGround);
	MC_CHECK_NEAR(z, 0.0, 1e-6);
}

MC_TEST(collide_sunk_into_floor_is_pushed_up_and_grounded)
{
	World w;
	for(int x = -2; x <= 2; x++)
		for(int y = -2; y <= 2; y++)
			w.Set(x, y, 0, BLOCK_STONE);     // floor occupies z in [0,1)
	float x = 0.5f, y = 0.5f, z = 0.9f;      // feet 0.1 below the floor surface
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(r.onGround);
	MC_CHECK_NEAR(z, 1.0, 1e-3);
	MC_CHECK_NEAR(x, 0.5, 1e-6);
	MC_CHECK_NEAR(y, 0.5, 1e-6);
}

MC_TEST(collide_standing_exactly_on_floor_is_not_moved)
{
	World w;
	for(int x = -2; x <= 2; x++)
		for(int y = -2; y <= 2; y++)
			w.Set(x, y, 0, BLOCK_STONE);
	float x = 0.5f, y = 0.5f, z = 1.0f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(!r.moved);
}

MC_TEST(collide_wall_pushes_sideways_not_up)
{
	World w;
	for(int z = 0; z < 4; z++)
		w.Set(1, 0, z, BLOCK_STONE);         // wall at x in [1,2)
	float x = 0.8f, y = 0.5f, z = 0.0f;      // box x range [0.5,1.1] overlaps the wall by 0.1
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(!r.onGround);
	MC_CHECK_NEAR(x, 0.7, 1e-3);             // pushed back to x + halfWidth == 1.0
	MC_CHECK_NEAR(z, 0.0, 1e-6);
}

MC_TEST(collide_ceiling_pushes_down)
{
	World w;
	for(int x = -2; x <= 2; x++)
		for(int y = -2; y <= 2; y++)
			w.Set(x, y, 2, BLOCK_STONE);     // ceiling occupies z in [2,3)
	float x = 0.5f, y = 0.5f, z = 0.3f;      // head at 2.1, 0.1 into the ceiling
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(!r.onGround);
	MC_CHECK_NEAR(z, 0.2, 1e-3);
}

MC_TEST(collide_works_in_negative_coordinates)
{
	World w;
	for(int x = -10; x <= -6; x++)
		for(int y = -10; y <= -6; y++)
			w.Set(x, y, -5, BLOCK_DIRT);     // floor top at z = -4
	float x = -8.5f, y = -8.5f, z = -4.1f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(r.onGround);
	MC_CHECK_NEAR(z, -4.0, 1e-3);
}

MC_TEST(collide_glass_is_solid)
{
	World w;
	w.Set(0, 0, 0, BLOCK_GLASS);
	float x = 0.5f, y = 0.5f, z = 0.9f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j`
Expected: FAIL at compile, `McCollide.h: No such file or directory`.

- [ ] **Step 3: Implement**

`src/minecraft/core/McCollide.h`:
```cpp
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
```

`src/minecraft/core/McCollide.cpp`:
```cpp
#include "McCollide.h"
#include <math.h>

namespace Mc {

static const float EPS = 1e-4f;

CollideResult PushOutOfBlocks(const World &w, float &px, float &py, float &pz, float halfWidth, float height)
{
	CollideResult res = { false, false };
	for(int iter = 0; iter < 8; iter++){
		float lo[3] = { px - halfWidth, py - halfWidth, pz };
		float hi[3] = { px + halfWidth, py + halfWidth, pz + height };

		bool pushed = false;
		int c0[3], c1[3];
		for(int a = 0; a < 3; a++){
			c0[a] = (int)floorf(lo[a] + EPS);
			c1[a] = (int)floorf(hi[a] - EPS);
		}
		for(int z = c0[2]; z <= c1[2] && !pushed; z++)
		for(int y = c0[1]; y <= c1[1] && !pushed; y++)
		for(int x = c0[0]; x <= c1[0] && !pushed; x++){
			if(w.Get(x, y, z) == BLOCK_AIR)
				continue;
			int cell[3] = { x, y, z };
			// penetration along each axis, and the push direction that resolves it
			float best = 1e30f;
			int bestAxis = -1;
			float bestPush = 0.0f;
			for(int a = 0; a < 3; a++){
				float overlapPos = hi[a] - (float)cell[a];		// push the box toward -a
				float overlapNeg = (float)(cell[a] + 1) - lo[a];	// push the box toward +a
				if(overlapPos <= EPS || overlapNeg <= EPS)
					continue;	// no overlap on this axis
				float pen = overlapPos < overlapNeg ? overlapPos : overlapNeg;
				if(pen < best){
					best = pen;
					bestAxis = a;
					bestPush = overlapPos < overlapNeg ? -overlapPos : overlapNeg;
				}
			}
			if(bestAxis < 0)
				continue;
			if(bestAxis == 0) px += bestPush;
			else if(bestAxis == 1) py += bestPush;
			else pz += bestPush;
			res.moved = true;
			res.onGround = (bestAxis == 2 && bestPush > 0.0f);
			pushed = true;
		}
		if(!pushed)
			break;
	}
	return res;
}

}
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build/mctest -j && build/mctest/mctests`
Expected: all tests pass (`42 test(s) passed`: 34 + 1 interact + 7 collide).

- [ ] **Step 5: Apply it to the player in game**

In `src/minecraft/game/McMode.cpp` add `#include "McCollide.h"`, `#include "PlayerInfo.h"`, `#include "Ped.h"`, and at the end of `Update` (after the `McInteract::Update` call) add:
```cpp
	if(active && FindPlayerPed() != nil){
		CPlayerPed *ped = FindPlayerPed();
		CVector pos = ped->GetPosition();
		// GTA positions the ped at its centre; the feet are about 1 unit lower
		float fx = pos.x, fy = pos.y, fz = pos.z - 1.0f;
		Mc::CollideResult r = Mc::PushOutOfBlocks(world, fx, fy, fz, 0.3f, 1.8f);
		if(r.moved){
			ped->SetPosition(fx, fy, fz + 1.0f);
			if(r.onGround){
				ped->bIsStanding = true;
				ped->m_vecMoveSpeed.z = 0.0f;
			}
		}
	}
```
Note on the ground offset: GTA peds' origin is at about 1.0 above the feet. If the player visibly floats or sinks when standing on a block, adjust the `1.0f` here (measure by standing on a placed block and comparing with the ground). This is the one constant to tune in game.

- [ ] **Step 6: Build and verify in game (manual)**

Run: `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Linking"`. In game with F8 on: build a 1-block step and walk onto it (Claude must climb/jump onto it and stand, not fall through); build a wall and run into it (must stop, no jitter); place a block above your head while standing and jump into it (must bump). Check that jumping still works while standing on a block. If Claude cannot jump from a block, the `bIsStanding`/`m_vecMoveSpeed.z` handling above is not enough; check `CPed::ProcessControl` for the condition at `Ped.cpp:1487` (`!bIsStanding`) and record findings in MODLOG.md.

- [ ] **Step 7: Commit (only if approved)**

```bash
git add tests/minecraft src/minecraft
git commit -m "feat(minecraft): voxel collision for the player"
```

---

### Task 9: Melee damage to peds

**Files:**
- Modify: `src/minecraft/game/McInteract.cpp`

**Interfaces:**
- Consumes: `CWorld::ProcessLineOfSight` (entity out-parameter), `CEntity::IsPed()`, `CPed::InflictDamage(CEntity*, eWeaponType, float, ePedPieceTypes, uint8)`.
- Produces: left click on a ped within reach, with no block closer than the ped, deals damage instead of breaking a block.

- [ ] **Step 1: Verify the exact names**

Run:
```
grep -n "WEAPONTYPE_BASEBALLBAT\|WEAPONTYPE_UNARMED" src/weapons/Weapon.h src/weapons/WeaponInfo.h
grep -n "PEDPIECE_TORSO" src/peds/*.h
grep -n "bool InflictDamage" src/peds/Ped.h
```
Expected: both enum values exist and `InflictDamage(CEntity*, eWeaponType, float, ePedPieceTypes, uint8)` is declared (`Ped.h:614`). Use the names that exist.

- [ ] **Step 2: Implement**

In `McInteract.cpp`, add `#include "Ped.h"` and `#include "Weapon.h"`, replace `GtaHitDistance` so it also returns the entity:
```cpp
static float
GtaHit(const CVector &from, const CVector &to, CEntity *&entity)
{
	CColPoint colPoint;
	entity = nil;
	if(CWorld::ProcessLineOfSight(from, to, colPoint, entity, true, true, true, true, false, true))
		return (colPoint.point - from).Magnitude();
	entity = nil;
	return reach + 1.0f;
}
```
and in `Update`, replace the block that calls `GtaHitDistance` and the `if(breakBlock)` branch with:
```cpp
	CEntity *gtaEntity;
	float gtaDist = GtaHit(origin, origin + dir * reach, gtaEntity);

	if(breakBlock && gtaEntity != nil && gtaEntity->IsPed() && (!hit.hit || gtaDist < hit.t)){
		CPed *ped = (CPed*)gtaEntity;
		ped->InflictDamage(FindPlayerPed(), WEAPONTYPE_BASEBALLBAT, 10.0f, PEDPIECE_TORSO, 0);
		return;
	}
	if(!hit.hit)
		return;
	if(gtaDist < hit.t)
		return;	// GTA geometry is in front of the block

	if(breakBlock){
		world.Set(hit.x, hit.y, hit.z, Mc::BLOCK_AIR);
	}else if(CanPlace(hit)){
		/* unchanged placement code from Task 7 */
	}
```
Keep the existing placement body exactly as written in Task 7 (the `px,py,pz` and `insidePlayer` logic); only the damage branch and the ordering change. Because a ped hit must work even when the voxel ray misses, move the `if(!hit.hit) return;` after the damage branch, as shown.

- [ ] **Step 3: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Linking"`
Expected: no `error`, ends with `Linking CXX executable src/re3`.

- [ ] **Step 4: Verify in game (manual)**

F8 on, aim at a pedestrian within 6 units and left click: the ped takes damage (10 per click; a few clicks should knock it down or kill it). With a placed block between you and the ped, left click must break the block and not hurt the ped. Right click on a ped must not place anything inside it (the placement still targets blocks or geometry, never the ped).

- [ ] **Step 5: Commit (only if approved)**

```bash
git add src/minecraft/game/McInteract.cpp
git commit -m "feat(minecraft): melee damage to peds"
```

---

## Self-Review (done while writing)

**Spec coverage:** `McWorld` (Tasks 1-2), `McMesher` (4), `McRenderer` (6), `McInteract` incl. DDA + `ProcessLineOfSight` + place/break/melee (3, 7, 9), `McPlayer` collision part (8), `McMode` toggle and lifecycle (5), persistence `mcworld.dat` (2, 5), four hooks behind `MINECRAFT_MODE` (5), error handling for corrupt saves (2) and placement inside the player (7), tests outside the game (1-4, 7-8). Explicitly deferred to plan 2: `McAtlas` and the texture-failure fallback path (this plan already always uses flat colours), Steve model, Minecraft-tuned jump/sprint, hotbar HUD, and phase 1b. The spec lists these under phase 1; the split is stated at the top of this plan.

**Type consistency:** `World::ChunkMap` is used by the renderer; `ChunkMesh`/`McVertex`/`MeshChunk(w, cp, textured, out)` match between Tasks 4 and 6; `RayHit`/`RayCast` match between Tasks 3, 7 and 9; `CollideResult`/`PushOutOfBlocks` match between Task 8 and its use in `McMode`; `McRenderer::Render(Mc::World&)` matches `McMode::Render`.

**Known risks to watch (recorded for the executor):**
1. GTA's own physics may not treat a voxel as ground (Task 8 Step 6): the ped can enter a falling state on blocks. The plan sets `bIsStanding`; if jumping from a block fails, investigate `CPed::ProcessControl`.
2. The ped-origin-to-feet offset (`1.0f`) in Task 8 is a guess to be tuned in game.
3. The render state sequence in Task 6 is copied in spirit from `Rubbish.cpp`; if blocks are invisible, compare state by state.
4. All in-game verification needs the user's GTA III install path, still unknown.
