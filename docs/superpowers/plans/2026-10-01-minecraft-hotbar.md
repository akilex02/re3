# On-screen hotbar — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Minecraft-style hotbar on screen in Steve mode, selectable with the wheel and number keys 1-4.

**Architecture:** Pure layout/mapping functions in the core with unit tests; a thin game-layer `McHotbar` that draws with re3's 2D sprite API from one new line in `Render2dStuff()`; `McInteract` exposes and sets the selected block.

**Tech Stack:** C++11, re3 `CSprite2d`/`CFont`/`CRect`/`CRGBA`, the existing `McAtlas` texture, the standalone test project in `tests/minecraft`.

**Spec:** `docs/superpowers/specs/2026-10-01-minecraft-hotbar-design.md`

## Global Constraints

- C++11 only. Core (`src/minecraft/core/`) uses only the standard library and is unit-tested in `tests/minecraft/` (never put test files under `src/`).
- Game layer (`src/minecraft/game/`) follows `CODING_STYLE.md`: tabs, return type on its own line, brace on the next line for function definitions, no braces around single statements, `int *ptr`, project typedefs (`uint8`, `int32`), `nil`. Everything inside `#ifdef MINECRAFT_MODE`. The include order that compiles for ped/world headers is `Ped.h`, `PlayerPed.h`, then `PlayerInfo.h`, `World.h`.
- The only Rockstar-file change is ONE line (plus an include inside an existing `#ifdef MINECRAFT_MODE` block) in `Render2dStuff()` of `src/core/main.cpp`. Everything else stays in `src/minecraft`. Never stage `src/core/config.h` (the user's uncommitted reformatting) or `CLAUDE.md`; stage by explicit path.
- Layout constants (verbatim from the spec): slot size = 6% of the screen height; gap = 15% of the slot size; bottom margin = 3.5% of the screen height; the selected slot is 10% bigger around the same centre; the bar is centred horizontally.
- Tile UVs: atlas 4x4 tiles, tile `(id % 4, id / 4)`, row 0 at the top, v grows downward; half-texel inset of `0.5 / 64` on each side to avoid bleeding.
- Run tests: `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j && timeout 120 build/mctest/mctests` (from `/home/akilex/Descargas/gtas/re3`; currently 119 pass). Game build: `cd build && cmake . && cmake --build . -j$(nproc)`. Do not run the game in implementer tasks (the user playtests).
- Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

- The bar must never draw outside Steve mode, in cutscenes/widescreen, when the player is dead or controls are disabled, or in menus.
- Render state must be left as `Render2dStuff` expects after the bar is drawn (no leaked texture raster, blend, or filter).
- Number keys must not fire in normal play (outside Steve mode) and must not disturb other GTA controls.
- Layout must stay centred and proportional for 16:9 and ultrawide aspect ratios and with 1 to N slots.

## File Structure

```
src/minecraft/core/McHotbarLayout.h/.cpp   slot rectangles, tile UVs, block <-> slot mapping
src/minecraft/game/McHotbar.h/.cpp         drawing
src/minecraft/game/McInteract.h/.cpp       GetSelectedBlock/SetSelectedBlock, keys 1-4
src/minecraft/game/McMode.h/.cpp           McMode::Render2d()
src/core/main.cpp                          one hook line in Render2dStuff()
tests/minecraft/test_hotbar.cpp
```

---

### Task 1: `McHotbarLayout` (core)

**Files:** Create `src/minecraft/core/McHotbarLayout.h`, `src/minecraft/core/McHotbarLayout.cpp`, `tests/minecraft/test_hotbar.cpp`.

**Interfaces (namespace `Mc`):**
- `struct HotbarSlot { float x, y, w, h; };` top-left corner and size in screen pixels.
- `HotbarSlot HotbarSlotRect(int index, int count, float screenW, float screenH, bool selected)`: `size = screenH*0.06`, `gap = size*0.15`, `total = count*size + (count-1)*gap`, `left = (screenW - total)/2`, `baseX = left + index*(size+gap)`, `baseY = screenH - screenH*0.035 - size`; unselected: `{baseX, baseY, size, size}`; selected: grown by 10% around the same centre: `{baseX - size*0.05, baseY - size*0.05, size*1.1, size*1.1}`. If `count <= 0` or `index < 0` or `index >= count` or `screenW <= 0` or `screenH <= 0` return `{0,0,0,0}`.
- `void BlockTileUV(uint8_t id, float &u0, float &v0, float &u1, float &v1)`: `tx = id % 4`, `ty = id / 4`, `u0 = tx/4 + inset`, `u1 = (tx+1)/4 - inset`, `v0 = ty/4 + inset`, `v1 = (ty+1)/4 - inset`, `inset = 0.5/64`.
- `int HotbarCount()` = `BLOCK_COUNT - 1`; `uint8_t HotbarBlock(int index)` = `index + 1` for `0 <= index < HotbarCount()`, else `BLOCK_AIR`; `int HotbarIndexOfBlock(uint8_t id)` = `id - 1` for `1 <= id < BLOCK_COUNT`, else `-1`.

- [ ] **Step 1: Write the failing tests** — `tests/minecraft/test_hotbar.cpp`:
```cpp
#include "mctest.h"
#include "McHotbarLayout.h"

using namespace Mc;

MC_TEST(hotbar_slot_positions_at_1080p)
{
	// size = 64.8, gap = 9.72, total = 4*64.8 + 3*9.72 = 288.36, left = (1920-288.36)/2 = 815.82
	HotbarSlot s0 = HotbarSlotRect(0, 4, 1920.0f, 1080.0f, false);
	HotbarSlot s1 = HotbarSlotRect(1, 4, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(s0.x, 815.82, 0.01);
	MC_CHECK_NEAR(s1.x, 815.82 + 64.8 + 9.72, 0.01);
	MC_CHECK_NEAR(s0.y, 1080.0 - 37.8 - 64.8, 0.01);	// 977.4
	MC_CHECK_NEAR(s0.w, 64.8, 0.01);
	MC_CHECK_NEAR(s0.h, 64.8, 0.01);
}

MC_TEST(hotbar_is_centred_and_symmetric)
{
	float screenW = 2560.0f, screenH = 1440.0f;
	HotbarSlot first = HotbarSlotRect(0, 4, screenW, screenH, false);
	HotbarSlot last = HotbarSlotRect(3, 4, screenW, screenH, false);
	float leftMargin = first.x;
	float rightMargin = screenW - (last.x + last.w);
	MC_CHECK_NEAR(leftMargin, rightMargin, 0.01);
}

MC_TEST(hotbar_scales_with_screen_height)
{
	HotbarSlot a = HotbarSlotRect(0, 4, 1920.0f, 1080.0f, false);
	HotbarSlot b = HotbarSlotRect(0, 4, 2560.0f, 1440.0f, false);
	MC_CHECK_NEAR(b.w / a.w, 1440.0 / 1080.0, 1e-4);
	// bottom margin is 3.5% of the height at both sizes
	MC_CHECK_NEAR((1080.0 - (a.y + a.h)) / 1080.0, 0.035, 1e-4);
	MC_CHECK_NEAR((1440.0 - (b.y + b.h)) / 1440.0, 0.035, 1e-4);
}

MC_TEST(hotbar_selected_slot_grows_around_the_same_centre)
{
	HotbarSlot n = HotbarSlotRect(2, 4, 1920.0f, 1080.0f, false);
	HotbarSlot s = HotbarSlotRect(2, 4, 1920.0f, 1080.0f, true);
	MC_CHECK_NEAR(s.w, n.w * 1.1, 0.01);
	MC_CHECK_NEAR(s.h, n.h * 1.1, 0.01);
	MC_CHECK_NEAR(s.x + s.w / 2, n.x + n.w / 2, 0.01);
	MC_CHECK_NEAR(s.y + s.h / 2, n.y + n.h / 2, 0.01);
}

MC_TEST(hotbar_single_slot_is_centred_and_ultrawide_is_centred)
{
	HotbarSlot one = HotbarSlotRect(0, 1, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(one.x + one.w / 2, 960.0, 0.01);
	HotbarSlot uw = HotbarSlotRect(1, 3, 3440.0f, 1440.0f, false);   // middle of 3
	MC_CHECK_NEAR(uw.x + uw.w / 2, 1720.0, 0.01);
}

MC_TEST(hotbar_invalid_arguments_give_empty_rect)
{
	HotbarSlot z = { 1, 1, 1, 1 };
	z = HotbarSlotRect(0, 0, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
	z = HotbarSlotRect(-1, 4, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
	z = HotbarSlotRect(4, 4, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
	z = HotbarSlotRect(0, 4, 0.0f, 1080.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
	z = HotbarSlotRect(0, 4, 1920.0f, -5.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
}

MC_TEST(hotbar_tile_uv_matches_the_atlas_layout)
{
	float u0, v0, u1, v1;
	const float inset = 0.5f / 64.0f;
	BlockTileUV(BLOCK_STONE, u0, v0, u1, v1);    // id 2 -> tile (2,0)
	MC_CHECK_NEAR(u0, 0.5 + inset, 1e-6);
	MC_CHECK_NEAR(u1, 0.75 - inset, 1e-6);
	MC_CHECK_NEAR(v0, 0.0 + inset, 1e-6);
	MC_CHECK_NEAR(v1, 0.25 - inset, 1e-6);
	BlockTileUV(5, u0, v0, u1, v1);              // id 5 -> tile (1,1)
	MC_CHECK_NEAR(u0, 0.25 + inset, 1e-6);
	MC_CHECK_NEAR(v0, 0.25 + inset, 1e-6);
	MC_CHECK_NEAR(v1, 0.5 - inset, 1e-6);
}

MC_TEST(hotbar_block_index_mapping)
{
	MC_CHECK_EQ(HotbarCount(), BLOCK_COUNT - 1);
	MC_CHECK_EQ(HotbarBlock(0), BLOCK_DIRT);
	MC_CHECK_EQ(HotbarBlock(HotbarCount() - 1), BLOCK_COUNT - 1);
	MC_CHECK_EQ(HotbarBlock(-1), BLOCK_AIR);
	MC_CHECK_EQ(HotbarBlock(HotbarCount()), BLOCK_AIR);
	MC_CHECK_EQ(HotbarIndexOfBlock(BLOCK_DIRT), 0);
	MC_CHECK_EQ(HotbarIndexOfBlock(BLOCK_GLASS), 3);
	MC_CHECK_EQ(HotbarIndexOfBlock(BLOCK_AIR), -1);
	MC_CHECK_EQ(HotbarIndexOfBlock(200), -1);
	for(int i = 0; i < HotbarCount(); i++)
		MC_CHECK_EQ(HotbarIndexOfBlock(HotbarBlock(i)), i);
}
```
- [ ] **Step 2: Run to verify it fails** — compile error (`McHotbarLayout.h` missing).
- [ ] **Step 3: Implement**

`src/minecraft/core/McHotbarLayout.h`:
```cpp
#pragma once
#include <stdint.h>
#include "McBlocks.h"

namespace Mc {

struct HotbarSlot {
	float x, y, w, h;	// top-left corner and size in screen pixels
};

// Slot `index` of `count` for a screen size. `selected` grows the slot by 10% around its centre.
// Invalid arguments give an empty rectangle.
HotbarSlot HotbarSlotRect(int index, int count, float screenW, float screenH, bool selected);

// UV rectangle of a block's tile in the 4x4 atlas (half-texel inset).
void BlockTileUV(uint8_t id, float &u0, float &v0, float &u1, float &v1);

int HotbarCount();
uint8_t HotbarBlock(int index);		// BLOCK_AIR if out of range
int HotbarIndexOfBlock(uint8_t id);	// -1 if not selectable

}
```
`src/minecraft/core/McHotbarLayout.cpp`:
```cpp
#include "McHotbarLayout.h"

namespace Mc {

HotbarSlot HotbarSlotRect(int index, int count, float screenW, float screenH, bool selected)
{
	HotbarSlot empty = { 0.0f, 0.0f, 0.0f, 0.0f };
	if(count <= 0 || index < 0 || index >= count || !(screenW > 0.0f) || !(screenH > 0.0f))
		return empty;
	float size = screenH * 0.06f;
	float gap = size * 0.15f;
	float total = (float)count * size + (float)(count - 1) * gap;
	float left = (screenW - total) * 0.5f;
	float baseX = left + (float)index * (size + gap);
	float baseY = screenH - screenH * 0.035f - size;
	HotbarSlot s = { baseX, baseY, size, size };
	if(selected){
		s.x = baseX - size * 0.05f;
		s.y = baseY - size * 0.05f;
		s.w = size * 1.1f;
		s.h = size * 1.1f;
	}
	return s;
}

void BlockTileUV(uint8_t id, float &u0, float &v0, float &u1, float &v1)
{
	const float inset = 0.5f / 64.0f;
	int tx = id % 4, ty = id / 4;
	u0 = (float)tx / 4.0f + inset;
	u1 = (float)(tx + 1) / 4.0f - inset;
	v0 = (float)ty / 4.0f + inset;
	v1 = (float)(ty + 1) / 4.0f - inset;
}

int HotbarCount()
{
	return BLOCK_COUNT - 1;
}

uint8_t HotbarBlock(int index)
{
	if(index < 0 || index >= HotbarCount())
		return BLOCK_AIR;
	return (uint8_t)(index + 1);
}

int HotbarIndexOfBlock(uint8_t id)
{
	if(id < 1 || id >= BLOCK_COUNT)
		return -1;
	return id - 1;
}

}
```
- [ ] **Step 4: Run to verify it passes** — all pass, no `-Wall -Wextra` warnings. If an expected number seems wrong, report the arithmetic; do not weaken tests.
- [ ] **Step 5: Commit** — stage the 3 new files; subject `feat(minecraft): hotbar layout`.

---

### Task 2: `McHotbar` drawing, keys 1-4 and the 2D hook

**Files:** Create `src/minecraft/game/McHotbar.h`, `src/minecraft/game/McHotbar.cpp`; Modify `src/minecraft/game/McInteract.h`, `src/minecraft/game/McInteract.cpp`, `src/minecraft/game/McMode.h`, `src/minecraft/game/McMode.cpp`, `src/core/main.cpp`.

**Interfaces:**
- Consumes: `Mc::HotbarSlotRect/BlockTileUV/HotbarCount/HotbarBlock/HotbarIndexOfBlock` (Task 1), `McAtlas::GetTexture()`, `Mc::GetBlockInfo`, re3 `CSprite2d`, `CFont`, `CRect`, `CRGBA`, `SCREEN_WIDTH`/`SCREEN_HEIGHT`, `McMode::IsActive()`.
- Produces: `McInteract::GetSelectedBlock()` (`uint8`) and `McInteract::SetSelectedBlock(uint8)` (ignores ids outside `1..BLOCK_COUNT-1`, prints the existing `selected block <name>` line); `McHotbar::Draw(void)`; `McMode::Render2d(void)`.

Behaviour:
1. `McInteract`: the static `selected` stays internal; add the accessors. Number keys `1`..`4` (use `HotbarCount()`; keys are `'1'..'1'+HotbarCount()-1`, at most 9) select slot `key - '1'` via `SetSelectedBlock(HotbarBlock(i))`. Read them as just-pressed from the pad key state (find the real field in `src/core/Pad.h`: `NewKeyState`/`OldKeyState` with `standardKeys` or an existing `GetStandardKeyJustDown`-like helper); keys act only inside `McInteract::Update`, i.e. only in Steve mode and only when the existing guard (no player, dead, cutscene, controls disabled, replay) passes. Before wiring them, grep `src/core/ControllerConfig.cpp` and `src/core/Pad.cpp` for number-key usage and report any conflict (the keys act only in Steve mode, so a conflict is acceptable but must be noted).
2. `McMode::Render2d()`: if `!IsActive()` return; same visibility guard as interaction (no player ped, player dead, cutscene running, controls disabled, replay playback, `TheCamera.m_WideScreenOn`); then `McHotbar::Draw()`. Declare it in `McMode.h`.
3. `McHotbar::Draw()`: for each slot `i` in `0..HotbarCount()-1`: block id `HotbarBlock(i)`, selected if it equals `McInteract::GetSelectedBlock()`; rectangle from `HotbarSlotRect(i, count, SCREEN_WIDTH, SCREEN_HEIGHT, selected)`; background with `CSprite2d::DrawRect(CRect(...), CRGBA(0,0,0,150))` (selected: also a 2-pixel (scaled) white frame drawn as four thin rects or a slightly larger white rect behind); the icon inset by about 12% of the slot: if `McAtlas::GetTexture()` is non-nil draw the tile UV rectangle of that texture, else `DrawRect` with the block's flat colour (`GetBlockInfo(id)` r,g,b, alpha 255). Find the way to draw a sub-rectangle of an `RwTexture` with `CSprite2d` (e.g. `SetTexture`/`Draw` overloads with UVs, or `CSprite2d::DrawRect` variants; if none supports custom UVs use `RwIm2D` immediates mirroring how `CSprite2d::Draw` is implemented in `src/core/Sprite2d.cpp`). Then the name of the selected block under the bar with `CFont` (small size, white with shadow; copy the CFont setup pattern from an existing HUD text such as `CHud` or `CPickups::RenderPickUpText`; centred horizontally under the bar; convert the name to the wide-char buffer `CFont` expects). The function must leave the render state as it found it (2D pass states: ztest off, zwrite off, vertex alpha on, blend src alpha / inv src alpha, cull none) and must not keep a texture raster bound: set the raster to nil when finished if the sprite code requires it.
4. Hook: in `Render2dStuff()` of `src/core/main.cpp`, after the HUD and the other 2D elements have been drawn and before the function ends (find the right place by reading the whole function; the hook must run after `CHud::Draw()` so the bar sits on top, and only on the normal path), add:
```cpp
#ifdef MINECRAFT_MODE
	McMode::Render2d();
#endif
```
`McMode.h` is already included in `main.cpp` under `#ifdef MINECRAFT_MODE`.

- [ ] **Step 1: Verify the APIs** (grep, record real names): `CSprite2d` draw functions and whether a textured UV sub-rect draw exists (`src/core/Sprite2d.h/.cpp`), `CFont` helpers (`SetFontStyle`, `SetScale`, `SetColor`, `SetDropShadowPosition`, `PrintString`, `SetCentreOn`, `SetWrapx` ...) used by `src/render/Hud.cpp` or `src/control/Pickups.cpp`, how those pass strings (`wchar*`), `CPad` key state fields and any just-pressed helper, `TheCamera.m_WideScreenOn`, `CCutsceneMgr::IsRunning`, `CPad::ArePlayerControlsDisabled`, `CReplay::IsPlayingBack`, and the end of `Render2dStuff()` / where `CHud::Draw()` is called.
- [ ] **Step 2: Implement** the interfaces and behaviour above.
- [ ] **Step 3: Build** — `cd build && cmake . && cmake --build . -j$(nproc)`: no `error`, final `Linking CXX executable src/re3`, no new warnings from `src/minecraft`; standalone tests still pass (127 expected: 119 + 8 hotbar tests; report the real number).
- [ ] **Step 4: Commit** — stage the changed/new files by explicit path (never `config.h`/`CLAUDE.md`); subject `feat(minecraft): on-screen hotbar and number keys`.
- Manual in-game checks (user, later): F8 shows the bar bottom centre with the right icons (textures when downloaded, flat colours otherwise); F8 off hides it; the wheel and keys 1-4 move the white frame; the name below matches; cutscenes hide it; same layout at two resolutions; blocks placed match the selected icon.

---

## Self-Review

**Spec coverage:** layout/constants (Task 1), icons from the atlas with flat fallback, selected frame and name, visibility guards, keys 1-4, accessor and the single hook line (Task 2). Out of scope exactly as in the spec.

**Type consistency:** `HotbarSlot`, `HotbarSlotRect`, `BlockTileUV`, `HotbarCount`, `HotbarBlock`, `HotbarIndexOfBlock` are defined in Task 1 and consumed by name in Task 2; `GetSelectedBlock/SetSelectedBlock` are `uint8` both in McInteract and McHotbar.

**Known risks for the executor:** (1) `CSprite2d` may not support a UV sub-rectangle of an arbitrary `RwTexture`: use the Im2D fallback described; (2) the 2D pass render state must be restored; (3) `CFont` takes wide strings and has global state (scale, colour, style): set what you need and restore or reset as the existing HUD code does.
