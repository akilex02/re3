# Minecraft mode: peds and vehicles collide with blocks (phase 1b) — design

Date: 2026-09-30
Status: approved by the user in conversation (approach A)
Parent spec: `2026-09-30-minecraft-mode-design.md` (phase 1; its "Collision" section defers this).

## Goal

Placed voxel blocks stop non-player peds and cars, like walls, so building a wall across a road blocks traffic and pedestrians. Blocks keep working outside Steve mode (they are part of the world).

## Approach (A: post-process push-out, game layer only)

No Rockstar files change. The existing `McMode::Update` hook (runs right after `CWorld::Process()`) gets one more call, `McEntities::Update(world)`, which resolves penetration for peds and vehicles that GTA's own collision cannot see.

Research basis (read-only exploration of re3): peds and vehicles are all in `CWorld::ms_listMovingEntityPtrs`; static-world collision is `CPhysical::ProcessCollision` sub-stepping + `ProcessEntityCollision`; none of it knows about voxels. Wheel suspension probes (`CAutomobile::ProcessEntityCollision`, `m_aWheelColPoints`) also ignore voxels, so a car resting on top of a block is held by the push-out but its wheels get no ground contact (accepted limitation; would need approach B).

## Units

| Unit | Responsibility | Where |
|---|---|---|
| `World::GetBounds` | Cell bounding box of all occupied chunks, for cheap culling | core + tests |
| `Mc::OrientedBox`, `BoxOverlapsBlocks`, `PushBoxOutOfBlocks` | Yaw-oriented box (vehicle) vs solid cells via 2D SAT + Z; minimal-translation push-out | core + tests |
| `Mc::SweepBox` | Moves a box along a path in steps of <= 0.4, returns the last free pose; stops fast vehicles tunnelling through 1-block walls | core + tests |
| `McEntities` | Iterates peds and vehicles, culls by bounds, applies push-out/sweep and velocity response | game |

## Behaviour

- Runs every frame after `CWorld::Process()`, only if the world has blocks. Cost with no blocks near an entity is a bounds test.
- Peds (not the player, who is handled by `McMode::UpdateGround`; not in a vehicle; with `bUsesCollision`): same body box as the player (half width 0.3, height 1.8, origin 1.0 above the feet), `PushOutOfBlocks`; velocity into the block is removed; if pushed upward: `bIsStanding = true`, vertical speed 0, `SetLanding()` if `bIsInTheAir`.
- Vehicles (cars only: not boats, trains, helicopters; including the player's own vehicle): oriented box from the colmodel `boundingBox` and the car's yaw; if the car moved more than 0.3 since last frame, sweep from the previous pose and stop at the last free pose; then `PushBoxOutOfBlocks`; velocity into the block is cancelled with a restitution of 0.2.
- If 8 push iterations do not free an entity (spawned inside blocks), it is left as is.

## Out of scope

AI avoidance of blocks, wheel/suspension contact on top of blocks (approach B), boats/trains/helicopters, objects (props), bullets/camera vs blocks (approach C).

## Testing

Pure unit tests in `tests/minecraft` for bounds, oriented push-out (yaw 0, 90, 45 degrees, floor, ceiling, negative coordinates, touching faces), and the sweep (fast mover stopped by a wall, no wall, parallel, starts inside). In game: a wall across a road with a car driven into it; pedestrians against a wall; a parked car beside blocks; the player's own car.
