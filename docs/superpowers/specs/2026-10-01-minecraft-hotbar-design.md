# Minecraft mode: on-screen hotbar — design

Date: 2026-10-01
Status: approved by the user in conversation (4 slots, bottom centre, Steve mode only, mouse wheel plus number keys 1-4)
Parent: `2026-09-30-minecraft-mode-design.md` (hotbar HUD, deferred from phase 1).

## Goal

In Steve mode (F8) the player sees the available blocks and the selected one on screen, instead of only a stdout line.

## Appearance

```
          [dirt] [stone] [WOOD] [glass]     4 slots, centred at the bottom
                    wood                    name of the selected block
```
- One slot per selectable block (ids `1..BLOCK_COUNT-1`, currently dirt, stone, wood, glass), semi-transparent black background, the block icon on top: the block's tile of the Mojang atlas when it exists, otherwise the block's flat colour.
- The selected slot is 10% bigger (same centre) with a white frame; its name is drawn below the bar in the game font with a shadow.
- Slot size is proportional to the screen height (6%), gap 15% of the slot, bottom margin 3.5% of the screen height: same look at 1080p and 1440p; centred so it clears the radar (bottom left) and the weapon icon (top right).
- Visible only when Steve mode is active and the game is in normal play: hidden in cutscenes, in widescreen/cinematic mode, when the player is dead or has no control (same conditions that disable block interaction).

## Input

- Mouse wheel: unchanged.
- Number keys `1`..`4` select the corresponding slot (slot index = key - 1), active only when Steve mode is on and the same interaction guards pass. If GTA III binds those keys to something in normal play (check the controller config) the keys only act in Steve mode and the conflict is noted in the report.

## Units

| Unit | Responsibility | Where |
|---|---|---|
| `McHotbarLayout` | Pure geometry: slot rectangles for a screen size, atlas UVs of a block tile (half-texel inset), block <-> slot index mapping | core + tests |
| `McHotbar` | Draws the bar with `CSprite2d` / `CFont` in the 2D pass | game |
| `McInteract` (changed) | `GetSelectedBlock()`, `SetSelectedBlock()`, number-key selection | game |
| `Render2dStuff()` hook | one line `McMode::Render2d()` under `#ifdef MINECRAFT_MODE`, the only Rockstar-file change of this feature | `src/core/main.cpp` |

## Out of scope

Item counts, drag and drop, more than the current blocks (the bar grows automatically with `BLOCK_COUNT`), animations, a crosshair.

## Testing

Unit tests for the layout (positions at 1920x1080 and 2560x1440, centring/symmetry, selected slot growth around the same centre, zero/negative counts), the UV rectangle of a tile, and the id/index mapping. In game: the bar appears with F8 and disappears with F8 off and in cutscenes, the wheel and keys 1-4 move the highlight, textures match the placed blocks, the layout is the same at two resolutions.
