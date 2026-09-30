# Minecraft mode for re3 (GTA III) — design

Date: 2026-09-30
Status: draft, pending user review

## Goal

A SkyCraft-style mashup for GTA III: the player can switch into "Steve" mode, walk through Liberty City with
Minecraft-style movement, place and break blocks in the GTA world, and hit peds. Personal, single-player,
offline project. Success = a recordable clip of Steve building in Liberty City and fighting peds.

Reference: https://github.com/chasmlol/SkyCraft (Minecraft in Skyrim via a hidden real Minecraft process,
shared memory and an SKSE plugin).

## Scope and phasing

- **Phase 1 (this spec):** native voxel layer inside re3 (approach B). One process, C++ only.
- **Phase 2 (later, separate spec):** bridge to a real Minecraft process (approach A, SkyCraft-style). It
  reuses the `McWorld` interface as the boundary: Minecraft would become another producer of block data.

Decisions taken with the user:
- Steve is a **mode toggled by a key**. The player stays Claude; the key swaps to Steve (model, Minecraft
  jump/sprint physics, hotbar, blocks) and back. GTA cars, missions and weapons stay intact outside the mode.
- Blocks live in a **custom voxel layer** (not `CObject`s, not sector collision boxes).
- Minecraft textures are **downloaded from Mojang on first launch**; nothing of Mojang's or Rockstar's is
  redistributed.

Out of scope for phase 1: redstone, fluids, Minecraft mobs, world generation, inventory crafting, multiplayer,
peds/vehicles colliding with blocks (phase 1b).

## Architecture

New directory `src/minecraft/`. `src/CMakeLists.txt` globs `*.cpp`/`*.h` recursively, so no build-file edits.
All integration with Rockstar code is four one-line calls behind `#ifdef MINECRAFT_MODE` (new switch in
`src/core/config.h`). No behaviour change when the flag is off.

| Unit | Responsibility | Depends on |
|---|---|---|
| `McWorld` | Sparse 16x16x16 chunks on a 1 m grid; `Get/Set(x,y,z)`; dirty flags; save/load | nothing |
| `McMesher` | Chunk to vertex/index buffers, hidden faces culled | `McWorld`, atlas UVs |
| `McRenderer` | Draws chunk meshes with `RwIm3D` inside GTA's frame | `McMesher`, librw |
| `McAtlas` | First-launch download from Mojang (curl), atlas load, flat-colour fallback | librw |
| `McPlayer` | Steve mode: physics, AABB collision vs voxels and GTA world, camera, hotbar | `McWorld`, `CPlayerPed` |
| `McInteract` | Crosshair ray (voxel DDA + `CWorld::ProcessLineOfSight`), place, break, melee | `McWorld`, `CWorld` |
| `McMode` | Toggle key, lifecycle; the only unit that touches GTA code | all above |

`McWorld` and `McMesher` know nothing about GTA and are unit-testable outside the game.

### Integration points (verified in the code)

1. `CGame::Process()` (`src/core/Game.cpp`) calls `McMode::Update()`.
2. `RenderEffects_new()` (`src/core/main.cpp`) calls `McRenderer::Render()` after the scene, so the depth
   buffer hides blocks correctly. `RwIm3DTransform` is already used this way in `src/renderer/Rubbish.cpp`.
3. `CPad` (`src/core/Pad.h`) supplies the toggle key and movement input.
4. Init/shutdown hooks where other systems initialise (`CGame::Initialise` / `ShutDown`).

## Data and flow

- 1 block = 1 GTA unit (about 1 m). Chunk index = `floor(coord/16)`. A chunk is created on first placement and
  freed when empty.
- Block = `uint8` id (0 air; small palette: dirt, stone, wood, glass, ...).
- Persistence: separate `mcworld.dat` next to the save. GTA's save format is untouched.

Per frame with the mode active:
1. `McMode::Update` reads input and advances `McPlayer`.
2. `McPlayer` applies Minecraft-style gravity/jump/sprint and moves Claude, colliding with two worlds: GTA
   (via `CWorld`) and the voxel grid (AABB vs grid).
3. `McInteract` casts from the camera; nearest of the GTA hit and the voxel DDA hit wins.
4. Left click: break the block, or damage a ped through GTA's weapon/damage path. Right click: place the
   hotbar block on the hit face.
5. Modified chunks are marked dirty and re-meshed before render.
6. `McRenderer` draws in `RenderEffects_new()`.

## Collision

- Phase 1: only Steve collides with blocks. Peds and vehicles pass through them.
- Phase 1b: hook `CPhysical` collision processing so peds and vehicles respect blocks.

## Error handling and edge cases

- Texture download fails: flat colour per block, warning in the log, no crash.
- Placement overlapping a ped, vehicle or the player: rejected.
- Leaving the mode keeps placed blocks in the world.
- Mode is disabled by default when `MINECRAFT_MODE` is off; vanilla behaviour is unchanged.

## Testing

- Unit tests outside the game for `McWorld` (get/set, chunk lifecycle, save/load round trip), `McMesher`
  (face culling) and the DDA (hit cell and face).
- In-game verification against the real game: vertical slice first (toggle, place one block, break it),
  then widen. `um win` automation is Windows-first, so on Linux verification is by screenshots, logs and manual
  checks. Needs the user's GTA III files (path still to be provided).
- Circuit breaker: the same failure three times means stop, record it, change approach.

## Build and project notes

- Existing CMake/Ninja build in `re3/build` works (`cmake --build build`).
- `src/core/config.h` currently has uncommitted user changes (looks like line endings). Do not touch them
  without asking; the `MINECRAFT_MODE` switch goes in as a minimal edit.
- Follow `CODING_STYLE.md` inside `src/minecraft/` (tabs, project typedefs `int32`/`uint8`, Hungarian
  naming). Never edit `vendor/`.

## Open items

- Path to the user's GTA III install (needed to run and verify).
- Toggle key binding.
- Exact block palette for the first slice (proposal: dirt, stone, wood, glass).
