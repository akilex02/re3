# Núcleo de supervivencia (MinecraftOSS) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bucle de supervivencia en el modo Steve de re3: minar con tiempo y herramienta, inventario 2x2, mesa de crafteo 3x3 y herramientas con desgaste, con las reglas de MinecraftOSS enlazadas como biblioteca Rust.

**Architecture:** Un crate `mc_bridge` (staticlib, API C) envuelve `minecraftoss-player` (`Inventory`, `Mining`, `RecipeBook`, `LootBook`). El mundo de voxels sigue en C++ y se expone a Rust con callbacks `get/set` por celda. Los ids numéricos de C++ se traducen a nombres `minecraft:*` con la tabla `McItemTable`. Toda la integración va tras `#ifdef MINECRAFT_SURVIVAL` dentro de `MINECRAFT_MODE`.

**Tech Stack:** C++11 (re3, CMake), Rust 1.98 (stable), `minecraftoss-player`, librw para el 2D. Tests: `mctests` (C++ puro) y `cargo test`.

**Spec:** `docs/superpowers/specs/2026-09-30-minecraft-survival-core-design.md`

## Cambios respecto al spec (hallados al leer el código; confirmar en la revisión)

1. **Solo `player`, no `core`.** `minecraftoss-player` no depende de `minecraftoss-core`.
2. **Sin copiar código de MinecraftOSS.** No hay `LICENSE` en `third_party/minecraftoss`. El crate se enlaza por ruta (`RE3_MINECRAFTOSS_DIR`, por defecto `../2010-rust-rewrite-mashup-main/third_party/minecraftoss`) mediante un symlink ignorado por git.
3. **Atlas 8x8** (64 tiles) en vez de 16x16: sobra para los 57 ids.
4. **Catálogo de minería generado en el puente** (`catalog.rs`) con dureza y nivel de herramienta de vanilla, porque el JSON del arnés de Fabric no existe en el mashup. Los drops salen del `LootBook` de Mojang (`client.jar`).
5. **Catálogo de items**: `ItemCatalog` necesita `item-catalog-26.3.json` (viene gzip en el mashup). Un script lo descomprime en `mcassets/`; no se versiona.
6. **Parche inicial (tecla F9)**: GTA no tiene troncos ni menas y no hay generador todavía. F9 construye junto al jugador un parche con árbol, piedra, menas, arena y grava para poder probar el ciclo.
7. **Progreso de minería con barra 2D**, no grietas en 10 etapas (requiere pasada de render propia). Los drops van directos al inventario (sin items en el suelo).
8. **Sin antorcha como bloque** (el mesher solo dibuja cubos). **Sin horno funcional**: el ciclo completo llega hasta la piedra; las herramientas de hierro se pueden fabricar si se tienen lingotes, pero obtenerlos exige fundir (fase siguiente).
9. **Sin arrastrar para repartir ni doble clic para recoger** (spec sección 1). MinecraftOSS tiene `Inventory::distribute`, `distribute_crafting` y `pickup_all`; un futuro `mc_inv_distribute` podría exponerlos.

## Global Constraints

- Estilo del repo: tabs, llave de función en línea nueva, `int *p`, tipos `uint8/int32` en código de re3; en `src/minecraft/core` solo biblioteca estándar (C++11).
- Todo bajo `#ifdef MINECRAFT_MODE`; el código de supervivencia además bajo `#ifdef MINECRAFT_SURVIVAL`. Ganchos en ficheros de Rockstar: ninguno nuevo.
- No tocar `src/core/config.h`, `vendor/`, ni `vendor/librw`. No versionar nada de Mojang (`mcassets/`).
- `rw::readPNG` asserta si falta el archivo: comprobar existencia antes. Filtro y direccionamiento de textura con ningún raster enlazado.
- Ninguna función `extern "C"` puede propagar un pánico: `catch_unwind` en todas.
- Coordenadas: GTA (x este, y norte, z arriba) ↔ MC (x este, y arriba, z sur): punto `mc=(gx,gz,-gy)`, celda `mc=(bx,bz,-by-1)`.
- Los ids 1..4 (dirt, stone, oak_planks, glass) conservan su valor: `mcworld.dat` existente sigue siendo válido.
- Commits con el trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. Minar una celda con id desconocido o aire: sin crash, sin drops (Task 3).
2. Inventario lleno al recoger un drop: `mc_inv_add` devuelve el sobrante y C++ lo avisa en el log, no lo pierde en silencio (Task 3, 4).
3. La herramienta se rompe: la ranura queda vacía y no se sigue minando con ella (Task 3).
4. Archivo de inventario ausente o corrupto: arranca vacío sin crash (Task 3).
5. Cerrar la pantalla con items en la rejilla o en el cursor: vuelven al inventario (Task 3, 5).
6. Punteros nulos o índices fuera de rango en la API C: devuelven error, no abortan (Task 3).
7. `cargo` ausente o catálogo ausente: el juego compila y arranca en modo Steve clásico (Task 1, 4).

## File Structure

```
src/minecraft/rust/mc_bridge/Cargo.toml        crate (staticlib+rlib)
src/minecraft/rust/mc_bridge/src/lib.rs        API C (extern "C") + guard
src/minecraft/rust/mc_bridge/src/coords.rs     GTA<->MC: puntos, celdas, yaw/pitch
src/minecraft/rust/mc_bridge/src/names.rs      tabla id numérico <-> nombre
src/minecraft/rust/mc_bridge/src/catalog.rs    JSON de minería generado
src/minecraft/rust/mc_bridge/src/world.rs      impl World sobre callbacks C
src/minecraft/rust/mc_bridge/src/survival.rs   McSurvival: inventario, minería, guardado
src/minecraft/rust/mc_bridge.h                 cabecera C escrita a mano
src/minecraft/core/McItemTable.{h,cpp}         57 items: nombre, textura, color
src/minecraft/core/McStarterPatch.{h,cpp}      parche de prueba (F9)
src/minecraft/core/McInventoryLayout.{h,cpp}   rectángulos de ranuras y hit-test
src/minecraft/game/McSurvival.{h,cpp}          envoltorio C++ de la API C
src/minecraft/game/McInventoryUI.{h,cpp}       pantalla E (2x2) y mesa (3x3)
scripts/minecraft/fetch-catalogs.sh            descomprime el catálogo de items
```

---

### Task 1: Rust + CMake + llamada de humo

**Files:**
- Create: `src/minecraft/rust/mc_bridge/Cargo.toml`, `src/minecraft/rust/mc_bridge/src/lib.rs`, `src/minecraft/rust/mc_bridge/src/coords.rs`, `src/minecraft/rust/mc_bridge.h`
- Modify: `src/CMakeLists.txt` (bloque Minecraft, líneas ~23-31), `.gitignore`, `src/minecraft/game/McMode.cpp` (`Init`)

**Interfaces:**
- Produces (Rust): `coords::{to_mc_point([f64;3])->DVec3, to_mc_cell([i32;3])->(i32,i32,i32), to_gta_cell((i32,i32,i32))->[i32;3], yaw_pitch([f64;3])->(f64,f64)}`; `extern "C" fn mc_bridge_version() -> *const c_char`.
- Produces (CMake): define `MINECRAFT_SURVIVAL` cuando el crate se compila; la `.a` se enlaza al ejecutable.

- [ ] **Step 1: Escribir `Cargo.toml`**

```toml
[package]
name = "mc_bridge"
version = "0.1.0"
edition = "2021"
publish = false

[lib]
crate-type = ["staticlib", "rlib"]

[dependencies]
minecraftoss-player = { path = "minecraftoss/player" }
glam = "0.29"
serde_json = "1"

[workspace]
```

- [ ] **Step 2: Escribir `coords.rs` con sus tests primero**

```rust
//! GTA (x east, y north, z up) <-> Minecraft (x east, y up, z south).
use glam::DVec3;

pub fn to_mc_point(g: [f64; 3]) -> DVec3 {
    DVec3::new(g[0], g[2], -g[1])
}
pub fn to_mc_cell(g: [i32; 3]) -> (i32, i32, i32) {
    (g[0], g[2], -g[1] - 1)
}
pub fn to_gta_cell(m: (i32, i32, i32)) -> [i32; 3] {
    [m.0, -m.2 - 1, m.1]
}
/// Minecraft yaw/pitch in degrees for a GTA direction (0 yaw south, positive pitch looks down).
pub fn yaw_pitch(dir: [f64; 3]) -> (f64, f64) {
    let m = to_mc_point(dir).normalize_or_zero();
    ((-m.x).atan2(m.z).to_degrees(), (-m.y).clamp(-1.0, 1.0).asin().to_degrees())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn cells_round_trip_including_negatives() {
        for g in [[0, 0, 0], [5, -7, 3], [-1, -1, -1], [100, 200, 50]] {
            assert_eq!(to_gta_cell(to_mc_cell(g)), g);
        }
    }
    #[test]
    fn cell_contains_its_point() {
        // point inside GTA cell (2,3,4) at (2.5,3.5,4.5) lands in the mapped MC cell
        let p = to_mc_point([2.5, 3.5, 4.5]);
        let c = to_mc_cell([2, 3, 4]);
        assert_eq!((p.x.floor() as i32, p.y.floor() as i32, p.z.floor() as i32), c);
    }
    #[test]
    fn directions_map_to_minecraft_yaw() {
        let (north, _) = yaw_pitch([0.0, 1.0, 0.0]);
        assert!((north.abs() - 180.0).abs() < 1e-9);
        let (east, _) = yaw_pitch([1.0, 0.0, 0.0]);
        assert!((east + 90.0).abs() < 1e-9);
        let (_, down) = yaw_pitch([0.0, 0.0, -1.0]);
        assert!((down - 90.0).abs() < 1e-9);
        let (_, up) = yaw_pitch([0.0, 0.0, 1.0]);
        assert!((up + 90.0).abs() < 1e-9);
    }
    #[test]
    fn zero_direction_does_not_panic() {
        let _ = yaw_pitch([0.0, 0.0, 0.0]);
    }
}
```

- [ ] **Step 3: `lib.rs` mínimo**

```rust
pub mod coords;

use std::os::raw::c_char;

#[no_mangle]
pub extern "C" fn mc_bridge_version() -> *const c_char {
    b"0.1.0\0".as_ptr() as *const c_char
}
```

- [ ] **Step 4: Crear el symlink a mano y comprobar que compila**

Run (desde la raíz del repo):
```
ln -sfn ../../../../2010-rust-rewrite-mashup-main/third_party/minecraftoss src/minecraft/rust/mc_bridge/minecraftoss
cd src/minecraft/rust/mc_bridge && cargo test 2>&1 | tail -20
```
Expected: 4 tests PASS. Si `core`-edición 2024 u otra dependencia fallase, anotar el error y parar a consultar (no editar el crate externo).

- [ ] **Step 5: `mc_bridge.h`**

```c
#ifndef MC_BRIDGE_H
#define MC_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif

const char *mc_bridge_version(void);

#ifdef __cplusplus
}
#endif
#endif
```

- [ ] **Step 6: CMake.** En `src/CMakeLists.txt`, tras la opción `${PROJECT}_MINECRAFT_MODE`, añadir:

```cmake
option(${PROJECT}_MINECRAFT_SURVIVAL "Survival core backed by MinecraftOSS (needs cargo)" ON)
set(${PROJECT}_MINECRAFTOSS_DIR "${CMAKE_SOURCE_DIR}/../2010-rust-rewrite-mashup-main/third_party/minecraftoss"
	CACHE PATH "MinecraftOSS checkout (needs player/Cargo.toml)")
```

y, después de `add_executable`, junto al bloque `if(${PROJECT}_MINECRAFT_MODE)`:

```cmake
set(MC_BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/minecraft/rust/mc_bridge")
find_program(MC_CARGO cargo)
if(${PROJECT}_MINECRAFT_MODE AND ${PROJECT}_MINECRAFT_SURVIVAL AND MC_CARGO
		AND EXISTS "${${PROJECT}_MINECRAFTOSS_DIR}/player/Cargo.toml")
	file(CREATE_LINK "${${PROJECT}_MINECRAFTOSS_DIR}" "${MC_BRIDGE_DIR}/minecraftoss" SYMBOLIC)
	set(MC_BRIDGE_LIB "${CMAKE_BINARY_DIR}/mc_bridge/release/libmc_bridge.a")
	add_custom_target(mc_bridge_rust ALL
		COMMAND ${MC_CARGO} build --release --manifest-path "${MC_BRIDGE_DIR}/Cargo.toml"
			--target-dir "${CMAKE_BINARY_DIR}/mc_bridge"
		BYPRODUCTS "${MC_BRIDGE_LIB}"
		COMMENT "Building the MinecraftOSS survival bridge")
	add_dependencies(${EXECUTABLE} mc_bridge_rust)
	target_compile_definitions(${EXECUTABLE} PRIVATE MINECRAFT_SURVIVAL)
	target_include_directories(${EXECUTABLE} PRIVATE "${MC_BRIDGE_DIR}/..")
	target_link_libraries(${EXECUTABLE} PRIVATE "${MC_BRIDGE_LIB}" ${CMAKE_DL_LIBS} m)
elseif(${PROJECT}_MINECRAFT_MODE AND ${PROJECT}_MINECRAFT_SURVIVAL)
	message(STATUS "Minecraft survival disabled: cargo or MinecraftOSS (${${PROJECT}_MINECRAFTOSS_DIR}) not found")
endif()
```

Añadir a `.gitignore`: `src/minecraft/rust/mc_bridge/minecraftoss`, `src/minecraft/rust/mc_bridge/target`, `src/minecraft/rust/mc_bridge/Cargo.lock` no (el lock sí se versiona).

- [ ] **Step 7: Llamada de humo.** En `McMode::Init`, tras `McAtlas::Init();`:

```cpp
#ifdef MINECRAFT_SURVIVAL
	printf("McMode: survival bridge %s\n", mc_bridge_version());
#endif
```
con `#ifdef MINECRAFT_SURVIVAL` + `#include "rust/mc_bridge.h"` arriba (include de `src/minecraft` ya está en el path).

- [ ] **Step 8: Compilar y ejecutar.**

Run: `cd build && cmake . && cmake --build . -j$(nproc) 2>&1 | tail -15`
Expected: compila; en el log del juego aparece `McMode: survival bridge 0.1.0`. Con `cargo` fuera del PATH (`PATH=/usr/bin/false`… o `-DRE3_MINECRAFT_SURVIVAL=OFF`) compila igual sin esa línea.

- [ ] **Step 9: Commit**

```bash
git add src/CMakeLists.txt .gitignore src/minecraft/rust src/minecraft/game/McMode.cpp
git commit -m "feat(minecraft): rust bridge skeleton linked into re3"
```

---

### Task 2: Tabla de items y atlas 8x8

**Files:**
- Create: `src/minecraft/core/McItemTable.h`, `src/minecraft/core/McItemTable.cpp`, `tests/minecraft/test_itemtable.cpp`
- Modify: `src/minecraft/core/McBlocks.h`, `src/minecraft/core/McBlocks.cpp`, `src/minecraft/core/McAtlasData.h` (`ATLAS_TILES`), `src/minecraft/core/McAtlasData.cpp` (`BlockTextureFile`, `ComposeAtlas`), `src/minecraft/core/McHotbarLayout.cpp` (`BlockTileUV`), `src/minecraft/game/McAtlas.cpp`, `tests/minecraft/test_atlasdata.cpp`, `tests/minecraft/test_hotbar.cpp`

**Interfaces:**
- Produces:
  - `Mc::ITEM_COUNT` (= 58), `Mc::LAST_BLOCK` (= 29), `Mc::BLOCK_COUNT` (= 30).
  - `struct Mc::ItemInfo { const char *name; const char *texture; uint8_t r, g, b; bool transparent; };`
  - `const ItemInfo &Mc::GetItemInfo(uint8_t id)` (id fuera de rango → entrada 0, aire).
  - `bool Mc::IsBlockItem(uint8_t id)`; `int Mc::FindItemByName(const char *name)` (-1 si no existe; nombre completo `minecraft:...`).
  - `const char *Mc::BlockTextureFile(uint8_t id)` ahora devuelve la ruta relativa a `assets/minecraft/textures/` (p. ej. `block/dirt.png`, `item/stick.png`).
  - `const char *Mc::TextureBasename(const char *path)`.

- [ ] **Step 1: Test de la tabla (falla)** `tests/minecraft/test_itemtable.cpp`

```cpp
#include "mctest.h"
#include <string.h>
#include "McItemTable.h"
#include "McBlocks.h"

using namespace Mc;

MC_TEST(itemtable_legacy_ids_are_stable)
{
	MC_CHECK(strcmp(GetItemInfo(BLOCK_DIRT).name, "minecraft:dirt") == 0);
	MC_CHECK(strcmp(GetItemInfo(BLOCK_STONE).name, "minecraft:stone") == 0);
	MC_CHECK(strcmp(GetItemInfo(BLOCK_WOOD).name, "minecraft:oak_planks") == 0);
	MC_CHECK(strcmp(GetItemInfo(BLOCK_GLASS).name, "minecraft:glass") == 0);
	MC_CHECK_EQ(BLOCK_DIRT, 1);
	MC_CHECK_EQ(BLOCK_GLASS, 4);
}

MC_TEST(itemtable_names_are_unique_and_namespaced)
{
	for(int i = 1; i < ITEM_COUNT; i++){
		MC_CHECK(strncmp(GetItemInfo(i).name, "minecraft:", 10) == 0);
		MC_CHECK(GetItemInfo(i).texture != nullptr);
		for(int j = i + 1; j < ITEM_COUNT; j++)
			MC_CHECK(strcmp(GetItemInfo(i).name, GetItemInfo(j).name) != 0);
	}
}

MC_TEST(itemtable_blocks_come_first)
{
	MC_CHECK(IsBlockItem(1));
	MC_CHECK(IsBlockItem(LAST_BLOCK));
	MC_CHECK(!IsBlockItem(LAST_BLOCK + 1));
	MC_CHECK(!IsBlockItem(0));
	MC_CHECK_EQ(BLOCK_COUNT, LAST_BLOCK + 1);
}

MC_TEST(itemtable_fits_the_atlas)
{
	MC_CHECK(ITEM_COUNT <= 8 * 8);
}

MC_TEST(itemtable_find_by_name)
{
	MC_CHECK_EQ(FindItemByName("minecraft:dirt"), BLOCK_DIRT);
	MC_CHECK(FindItemByName("minecraft:stick") > LAST_BLOCK);
	MC_CHECK_EQ(FindItemByName("minecraft:nope"), -1);
	MC_CHECK_EQ(FindItemByName(nullptr), -1);
}

MC_TEST(itemtable_out_of_range_is_air)
{
	MC_CHECK(strcmp(GetItemInfo(ITEM_COUNT).name, "minecraft:air") == 0);
	MC_CHECK(strcmp(GetItemInfo(255).name, "minecraft:air") == 0);
}

MC_TEST(itemtable_basename)
{
	MC_CHECK(strcmp(TextureBasename("block/dirt.png"), "dirt.png") == 0);
	MC_CHECK(strcmp(TextureBasename("stick.png"), "stick.png") == 0);
}
```

Run: `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j 2>&1 | tail -5`. Expected: FAIL (no existe `McItemTable.h`).

- [ ] **Step 2: `McItemTable.h`**

```cpp
#pragma once
#include <stdint.h>

namespace Mc {

// ids 1..LAST_BLOCK are placeable blocks (ids 1..4 are the original voxel ids), LAST_BLOCK+1..ITEM_COUNT-1 are items.
const int LAST_BLOCK = 29;
const int ITEM_COUNT = 58;

struct ItemInfo {
	const char *name;	// full id, e.g. "minecraft:oak_planks"
	const char *texture;	// path under assets/minecraft/textures/
	uint8_t r, g, b;	// flat colour when there is no atlas
	bool transparent;
};

// Ids outside 0..ITEM_COUNT-1 return the air entry.
const ItemInfo &GetItemInfo(uint8_t id);
bool IsBlockItem(uint8_t id);
// -1 when unknown or null.
int FindItemByName(const char *name);
// "block/dirt.png" -> "dirt.png"
const char *TextureBasename(const char *path);

}
```

- [ ] **Step 3: `McItemTable.cpp` con la tabla completa**

```cpp
#include <string.h>
#include "McItemTable.h"

namespace Mc {

static const ItemInfo items[ITEM_COUNT] = {
	{ "minecraft:air", nullptr, 0, 0, 0, true },
	// blocks (1..29); 1..4 keep their historical ids
	{ "minecraft:dirt", "block/dirt.png", 134, 96, 67, false },
	{ "minecraft:stone", "block/stone.png", 125, 125, 125, false },
	{ "minecraft:oak_planks", "block/oak_planks.png", 160, 130, 80, false },
	{ "minecraft:glass", "block/glass.png", 190, 225, 235, true },
	{ "minecraft:grass_block", "block/grass_block_side.png", 95, 159, 53, false },
	{ "minecraft:cobblestone", "block/cobblestone.png", 110, 110, 110, false },
	{ "minecraft:sand", "block/sand.png", 219, 207, 163, false },
	{ "minecraft:gravel", "block/gravel.png", 131, 127, 126, false },
	{ "minecraft:oak_log", "block/oak_log.png", 102, 81, 50, false },
	{ "minecraft:oak_leaves", "block/oak_leaves.png", 60, 130, 40, true },
	{ "minecraft:coal_ore", "block/coal_ore.png", 105, 105, 105, false },
	{ "minecraft:iron_ore", "block/iron_ore.png", 136, 115, 100, false },
	{ "minecraft:crafting_table", "block/crafting_table_front.png", 143, 100, 55, false },
	{ "minecraft:furnace", "block/furnace_front.png", 120, 120, 120, false },
	{ "minecraft:birch_log", "block/birch_log.png", 216, 215, 210, false },
	{ "minecraft:birch_planks", "block/birch_planks.png", 192, 175, 121, false },
	{ "minecraft:spruce_log", "block/spruce_log.png", 58, 37, 16, false },
	{ "minecraft:spruce_planks", "block/spruce_planks.png", 114, 84, 48, false },
	{ "minecraft:bricks", "block/bricks.png", 150, 97, 83, false },
	{ "minecraft:sandstone", "block/sandstone.png", 216, 203, 155, false },
	{ "minecraft:stone_bricks", "block/stone_bricks.png", 122, 122, 122, false },
	{ "minecraft:coal_block", "block/coal_block.png", 16, 16, 16, false },
	{ "minecraft:iron_block", "block/iron_block.png", 220, 220, 220, false },
	{ "minecraft:diamond_ore", "block/diamond_ore.png", 120, 150, 150, false },
	{ "minecraft:diamond_block", "block/diamond_block.png", 98, 237, 228, false },
	{ "minecraft:gold_ore", "block/gold_ore.png", 145, 135, 100, false },
	{ "minecraft:gold_block", "block/gold_block.png", 246, 208, 61, false },
	{ "minecraft:white_wool", "block/white_wool.png", 233, 236, 236, false },
	{ "minecraft:obsidian", "block/obsidian.png", 20, 18, 30, false },
	// items (30..57)
	{ "minecraft:stick", "item/stick.png", 130, 100, 50, false },
	{ "minecraft:coal", "item/coal.png", 30, 30, 30, false },
	{ "minecraft:iron_ingot", "item/iron_ingot.png", 216, 216, 216, false },
	{ "minecraft:raw_iron", "item/raw_iron.png", 200, 160, 130, false },
	{ "minecraft:diamond", "item/diamond.png", 90, 230, 220, false },
	{ "minecraft:gold_ingot", "item/gold_ingot.png", 246, 208, 61, false },
	{ "minecraft:raw_gold", "item/raw_gold.png", 220, 180, 60, false },
	{ "minecraft:flint", "item/flint.png", 60, 60, 60, false },
	{ "minecraft:wooden_pickaxe", "item/wooden_pickaxe.png", 160, 130, 80, false },
	{ "minecraft:wooden_axe", "item/wooden_axe.png", 160, 130, 80, false },
	{ "minecraft:wooden_shovel", "item/wooden_shovel.png", 160, 130, 80, false },
	{ "minecraft:wooden_hoe", "item/wooden_hoe.png", 160, 130, 80, false },
	{ "minecraft:wooden_sword", "item/wooden_sword.png", 160, 130, 80, false },
	{ "minecraft:stone_pickaxe", "item/stone_pickaxe.png", 125, 125, 125, false },
	{ "minecraft:stone_axe", "item/stone_axe.png", 125, 125, 125, false },
	{ "minecraft:stone_shovel", "item/stone_shovel.png", 125, 125, 125, false },
	{ "minecraft:stone_hoe", "item/stone_hoe.png", 125, 125, 125, false },
	{ "minecraft:stone_sword", "item/stone_sword.png", 125, 125, 125, false },
	{ "minecraft:iron_pickaxe", "item/iron_pickaxe.png", 216, 216, 216, false },
	{ "minecraft:iron_axe", "item/iron_axe.png", 216, 216, 216, false },
	{ "minecraft:iron_shovel", "item/iron_shovel.png", 216, 216, 216, false },
	{ "minecraft:iron_hoe", "item/iron_hoe.png", 216, 216, 216, false },
	{ "minecraft:iron_sword", "item/iron_sword.png", 216, 216, 216, false },
	{ "minecraft:diamond_pickaxe", "item/diamond_pickaxe.png", 90, 230, 220, false },
	{ "minecraft:diamond_axe", "item/diamond_axe.png", 90, 230, 220, false },
	{ "minecraft:diamond_shovel", "item/diamond_shovel.png", 90, 230, 220, false },
	{ "minecraft:diamond_hoe", "item/diamond_hoe.png", 90, 230, 220, false },
	{ "minecraft:diamond_sword", "item/diamond_sword.png", 90, 230, 220, false },
};

const ItemInfo &GetItemInfo(uint8_t id)
{
	if(id >= ITEM_COUNT)
		return items[0];
	return items[id];
}

bool IsBlockItem(uint8_t id)
{
	return id >= 1 && id <= LAST_BLOCK;
}

int FindItemByName(const char *name)
{
	if(name == nullptr)
		return -1;
	for(int i = 1; i < ITEM_COUNT; i++)
		if(strcmp(items[i].name, name) == 0)
			return i;
	return -1;
}

const char *TextureBasename(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

}
```

- [ ] **Step 4: `McBlocks`.** En `McBlocks.h` poner `BLOCK_COUNT = 30` (= `LAST_BLOCK + 1`) y eliminar `BLOCK_COUNT` del enum dejando `BLOCK_AIR=0, BLOCK_DIRT, BLOCK_STONE, BLOCK_WOOD, BLOCK_GLASS`; declarar `const int BLOCK_COUNT = 30;` en el namespace. `McBlocks.cpp`: `GetBlockInfo` construye su tabla estática desde `GetItemInfo` en la primera llamada:

```cpp
#include "McBlocks.h"
#include "McItemTable.h"

namespace Mc {

const BlockInfo &GetBlockInfo(uint8_t id)
{
	static BlockInfo table[BLOCK_COUNT];
	static bool built = false;
	if(!built){
		for(int i = 0; i < BLOCK_COUNT; i++){
			const ItemInfo &it = GetItemInfo((uint8_t)i);
			BlockInfo b = { it.name, it.r, it.g, it.b, it.transparent };
			table[i] = b;
		}
		built = true;
	}
	return table[id < BLOCK_COUNT ? id : BLOCK_AIR];
}

}
```
Los tests existentes que compararan nombres como `"wood"` se actualizan a los nombres vanilla.

- [ ] **Step 5: Atlas.** `McAtlasData.h`: `const int ATLAS_TILES = 8;` (atlas 128x128). `BlockTextureFile` en `McAtlasData.cpp` pasa a `return GetItemInfo(id).texture;`. En `McHotbarLayout.cpp` `BlockTileUV`: `inset = 0.5f / 128.0f`, `tx = id % 8, ty = id / 8`, divisores `8.0f`. `HotbarCount/HotbarBlock/HotbarIndexOfBlock` siguen sobre `BLOCK_COUNT-1` (modo clásico). Actualizar `test_atlasdata.cpp` (`ATLAS_PIXELS` 128, magenta para `id >= ITEM_COUNT`) y `test_hotbar.cpp` (UV con rejilla 8x8; los valores `/4.0`→`/8.0`, inset `0.5/128`).

- [ ] **Step 6: `McAtlas.cpp`.** Sustituir `atlasBlocks[]` por un bucle sobre ids `1..ITEM_COUNT-1`:
  - `jarTextureDir` pasa a `"assets/minecraft/textures/"` y el comando `unzip` añade `jarTextureDir + BlockTextureFile(id)` por id.
  - Las rutas locales usan `Mc::TextureBasename(Mc::BlockTextureFile(id))` (unzip `-j` aplana).
  - `static_assert(Mc::ITEM_COUNT <= Mc::ATLAS_TILES * Mc::ATLAS_TILES, ...)`.
  - `tiles[id]` se rellena para cada id; los inválidos quedan con el color plano de `GetItemInfo`.
  Comprobar antes que cada textura existe en el jar: `unzip -l mcassets/client.jar | grep -c "textures/\(block\|item\)/"` y, para cada ruta de la tabla, `unzip -l mcassets/client.jar "assets/minecraft/textures/<ruta>"`. Si alguna no existe en 26.3, corregir la fila de la tabla (nombre vigente en el jar) y volver a probar.

- [ ] **Step 7: Run tests**

Run: `cmake --build build/mctest -j && build/mctest/mctests`
Expected: todos PASS (127 anteriores adaptados + los nuevos).

- [ ] **Step 8: Compilar el juego, probar F8 y comprobar las 4 texturas clásicas**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -5`, ejecutar el juego como indica el HANDOFF; F8, colocar bloques: las texturas de dirt/stone/planks/glass se ven como antes y `mcworld.dat` anterior carga.

- [ ] **Step 9: Commit**

```bash
git add src/minecraft tests/minecraft
git commit -m "feat(minecraft): item table with 57 entries and 8x8 atlas"
```

---

### Task 3: Núcleo de supervivencia en Rust (sin interfaz)

**Files:**
- Create: `src/minecraft/rust/mc_bridge/src/{names,catalog,world,survival}.rs`, `scripts/minecraft/fetch-catalogs.sh`
- Modify: `src/minecraft/rust/mc_bridge/src/lib.rs`, `src/minecraft/rust/mc_bridge.h`, `.gitignore`

**Interfaces:**
- Consumes: `coords::*` (Task 1); lista de nombres de `McItemTable` (índice = id, 0 = `minecraft:air`).
- Produces (C, en `mc_bridge.h`):

```c
typedef struct McSurvival McSurvival;
typedef struct { int32_t item; int32_t count; int32_t damage; int32_t max_damage; } McStack; /* item 0 = vacío */
typedef struct { int32_t broken; int32_t block_id; float progress; int32_t drops_added; int32_t tool_broke; } McMineResult;
typedef int32_t (*McGetBlockFn)(void *ctx, int32_t x, int32_t y, int32_t z);   /* celda GTA -> id */
typedef void (*McSetBlockFn)(void *ctx, int32_t x, int32_t y, int32_t z, int32_t id);

McSurvival *mc_survival_create(const char *client_jar, const char *item_catalog_json,
                               const char *const *item_names, int32_t item_count);
void mc_survival_destroy(McSurvival *s);
const char *mc_survival_last_error(void);          /* del último create fallido; nunca NULL */

/* area: 0 inventario (0..8 hotbar, 9..35 mochila, 36..39 armadura), 1 rejilla 2x2, 2 rejilla 3x3, 3 cursor, 4 salida 2x2, 5 salida 3x3 */
int32_t mc_inv_get(McSurvival *s, int32_t area, int32_t index, McStack *out);   /* 1 si hay item, 0 vacío/error */
int32_t mc_inv_click(McSurvival *s, int32_t area, int32_t index, int32_t right, int32_t shift);  /* 1 ok */
int32_t mc_inv_take_output(McSurvival *s, int32_t workbench, int32_t shift);    /* 1 si fabricó */
int32_t mc_inv_close(McSurvival *s);               /* devuelve rejilla y cursor al inventario; sobrantes que no caben: nº de stacks perdidos */
int32_t mc_inv_add(McSurvival *s, int32_t item, int32_t count);                 /* sobrante que no cupo; -1 si error */
int32_t mc_inv_consume(McSurvival *s, int32_t slot, int32_t count);             /* 1 si quitó */

int32_t mc_survival_mine(McSurvival *s, McGetBlockFn get, McSetBlockFn set, void *ctx,
                         const double eye[3], const double dir[3], int32_t attacking, int32_t on_ground,
                         int32_t selected_slot, McMineResult *out);              /* un tick de 1/20 s */
void mc_survival_stop_mining(McSurvival *s);

int32_t mc_survival_save(McSurvival *s, const char *path);                       /* 1 ok */
int32_t mc_survival_load(McSurvival *s, const char *path);                       /* 1 ok; 0 si falta o está corrupto (queda vacío) */
```

- [ ] **Step 1: Script del catálogo de items.** `scripts/minecraft/fetch-catalogs.sh`:

```sh
#!/bin/sh
# Unpacks the item catalog MinecraftOSS ships (gzip) beside the downloaded Mojang assets. Run from the game folder.
set -e
SRC="${1:-/home/akilex/Descargas/gtas/2010-rust-rewrite-mashup-main/crates/assets/data/minecraft/item-catalog-26.3.json.gz}"
mkdir -p mcassets
gunzip -c "$SRC" > mcassets/item-catalog-26.3.json
echo "wrote mcassets/item-catalog-26.3.json"
```
Run: `cd "/mnt/1TB/Juegos/GTAs/0. GTA III/0. TEST" && sh /home/akilex/Descargas/gtas/re3/scripts/minecraft/fetch-catalogs.sh`. Expected: archivo creado, y `head -c 200 mcassets/item-catalog-26.3.json` muestra `"schema_version":1`.

- [ ] **Step 2: `names.rs` (test primero)**

```rust
use std::collections::HashMap;

pub struct Names {
    by_id: Vec<String>,
    by_name: HashMap<String, i32>,
    last_block: i32,
}

impl Names {
    /// `names[0]` is air. Ids `1..=last_block` are placeable blocks.
    pub fn new(names: Vec<String>, last_block: i32) -> Self {
        let by_name = names.iter().enumerate().map(|(i, n)| (n.clone(), i as i32)).collect();
        Self { by_id: names, by_name, last_block }
    }
    pub fn name(&self, id: i32) -> Option<&str> {
        if id <= 0 { return None; }
        self.by_id.get(id as usize).map(String::as_str)
    }
    pub fn id(&self, name: &str) -> Option<i32> {
        self.by_name.get(name).copied().filter(|&i| i > 0)
    }
    pub fn is_block(&self, id: i32) -> bool {
        id >= 1 && id <= self.last_block
    }
    pub fn len(&self) -> usize { self.by_id.len() }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn sample() -> Names {
        Names::new(vec!["minecraft:air".into(), "minecraft:dirt".into(), "minecraft:stick".into()], 1)
    }
    #[test]
    fn lookup_both_ways() {
        let n = sample();
        assert_eq!(n.name(1), Some("minecraft:dirt"));
        assert_eq!(n.id("minecraft:stick"), Some(2));
    }
    #[test]
    fn air_unknown_and_negative_are_none() {
        let n = sample();
        assert_eq!(n.name(0), None);
        assert_eq!(n.name(-3), None);
        assert_eq!(n.name(99), None);
        assert_eq!(n.id("minecraft:air"), None);
        assert_eq!(n.id("minecraft:nope"), None);
    }
    #[test]
    fn blocks_versus_items() {
        let n = sample();
        assert!(n.is_block(1));
        assert!(!n.is_block(2));
        assert!(!n.is_block(0));
    }
}
```

- [ ] **Step 3: `catalog.rs` (test primero).** Antes de escribirlo leer `exact_f32` en `third_party/minecraftoss/player/src/mining.rs` para saber el formato exacto de dureza y velocidad (número o texto) y generar el JSON en ese formato. Contenido:

```rust
//! Mining catalog (hardness, tool tier, tool speed) for the curated blocks, in the JSON schema
//! `MiningCatalog::from_slice` reads. Values are vanilla 26.3.
use serde_json::{json, Map, Value};

struct Row { name: &'static str, hardness: f64, tool: &'static str, min_tier: u8, requires_tool: bool }

const TIERS: [(&str, u8, f64); 4] = [("wooden", 0, 2.0), ("stone", 1, 4.0), ("iron", 2, 6.0), ("diamond", 3, 8.0)];
const KINDS: [&str; 5] = ["pickaxe", "axe", "shovel", "hoe", "sword"];

const ROWS: &[Row] = &[
    Row { name: "dirt", hardness: 0.5, tool: "shovel", min_tier: 0, requires_tool: false },
    Row { name: "stone", hardness: 1.5, tool: "pickaxe", min_tier: 0, requires_tool: true },
    Row { name: "oak_planks", hardness: 2.0, tool: "axe", min_tier: 0, requires_tool: false },
    Row { name: "glass", hardness: 0.3, tool: "none", min_tier: 0, requires_tool: false },
    Row { name: "grass_block", hardness: 0.6, tool: "shovel", min_tier: 0, requires_tool: false },
    Row { name: "cobblestone", hardness: 2.0, tool: "pickaxe", min_tier: 0, requires_tool: true },
    Row { name: "sand", hardness: 0.5, tool: "shovel", min_tier: 0, requires_tool: false },
    Row { name: "gravel", hardness: 0.6, tool: "shovel", min_tier: 0, requires_tool: false },
    Row { name: "oak_log", hardness: 2.0, tool: "axe", min_tier: 0, requires_tool: false },
    Row { name: "oak_leaves", hardness: 0.2, tool: "hoe", min_tier: 0, requires_tool: false },
    Row { name: "coal_ore", hardness: 3.0, tool: "pickaxe", min_tier: 0, requires_tool: true },
    Row { name: "iron_ore", hardness: 3.0, tool: "pickaxe", min_tier: 1, requires_tool: true },
    Row { name: "crafting_table", hardness: 2.5, tool: "axe", min_tier: 0, requires_tool: false },
    Row { name: "furnace", hardness: 3.5, tool: "pickaxe", min_tier: 0, requires_tool: true },
    Row { name: "birch_log", hardness: 2.0, tool: "axe", min_tier: 0, requires_tool: false },
    Row { name: "birch_planks", hardness: 2.0, tool: "axe", min_tier: 0, requires_tool: false },
    Row { name: "spruce_log", hardness: 2.0, tool: "axe", min_tier: 0, requires_tool: false },
    Row { name: "spruce_planks", hardness: 2.0, tool: "axe", min_tier: 0, requires_tool: false },
    Row { name: "bricks", hardness: 2.0, tool: "pickaxe", min_tier: 0, requires_tool: true },
    Row { name: "sandstone", hardness: 0.8, tool: "pickaxe", min_tier: 0, requires_tool: true },
    Row { name: "stone_bricks", hardness: 1.5, tool: "pickaxe", min_tier: 0, requires_tool: true },
    Row { name: "coal_block", hardness: 5.0, tool: "pickaxe", min_tier: 0, requires_tool: true },
    Row { name: "iron_block", hardness: 5.0, tool: "pickaxe", min_tier: 1, requires_tool: true },
    Row { name: "diamond_ore", hardness: 3.0, tool: "pickaxe", min_tier: 2, requires_tool: true },
    Row { name: "diamond_block", hardness: 5.0, tool: "pickaxe", min_tier: 2, requires_tool: true },
    Row { name: "gold_ore", hardness: 3.0, tool: "pickaxe", min_tier: 2, requires_tool: true },
    Row { name: "gold_block", hardness: 3.0, tool: "pickaxe", min_tier: 2, requires_tool: true },
    Row { name: "white_wool", hardness: 0.8, tool: "none", min_tier: 0, requires_tool: false },
    Row { name: "obsidian", hardness: 50.0, tool: "pickaxe", min_tier: 3, requires_tool: true },
];

pub fn mining_catalog_json() -> Vec<u8> {
    let mut blocks = Map::new();
    for row in ROWS {
        let mut tools = Map::new();
        for (tier, level, speed) in TIERS {
            for kind in KINDS {
                let matches = kind == row.tool;
                tools.insert(
                    format!("minecraft:{tier}_{kind}"),
                    json!({ "speed": if matches { speed } else { 1.0 }, "correct": matches && level >= row.min_tier }),
                );
            }
        }
        blocks.insert(
            format!("minecraft:{}", row.name),
            json!({ "hardness": row.hardness, "requires_tool": row.requires_tool, "tools": Value::Object(tools) }),
        );
    }
    serde_json::to_vec(&json!({ "schema_version": 1, "minecraft_version": "26.3", "blocks": Value::Object(blocks) })).unwrap()
}

#[cfg(test)]
mod tests {
    use super::*;
    use minecraftoss_player::mining::MiningCatalog;

    #[test]
    fn catalog_parses_with_the_real_reader() {
        let catalog = MiningCatalog::from_slice(&mining_catalog_json()).unwrap();
        assert_eq!(catalog.block_count(), ROWS.len());
    }
    #[test]
    fn every_row_has_a_positive_hardness() {
        assert!(ROWS.iter().all(|r| r.hardness > 0.0));
    }
}
```
Run `cargo test catalog` hasta que pase; ajustar el formato numérico si `exact_f32` pide otra cosa.

- [ ] **Step 4: `world.rs`**

```rust
use crate::coords::{to_gta_cell};
use crate::names::Names;
use minecraftoss_player::{Block, Pos, World};
use std::os::raw::c_void;

pub type GetFn = unsafe extern "C" fn(*mut c_void, i32, i32, i32) -> i32;
pub type SetFn = unsafe extern "C" fn(*mut c_void, i32, i32, i32, i32);

pub struct CWorld<'a> {
    pub get: GetFn,
    pub set: SetFn,
    pub ctx: *mut c_void,
    pub names: &'a Names,
}

impl World for CWorld<'_> {
    fn block(&self, pos: Pos) -> Option<Block> {
        let g = to_gta_cell(pos);
        let id = unsafe { (self.get)(self.ctx, g[0], g[1], g[2]) };
        if !self.names.is_block(id) { return None; }
        self.names.name(id).map(Block::new)
    }
    fn set_block(&mut self, pos: Pos, block: Option<Block>) {
        let g = to_gta_cell(pos);
        let id = block.and_then(|b| self.names.id(&b.id)).unwrap_or(0);
        unsafe { (self.set)(self.ctx, g[0], g[1], g[2], id) };
    }
}
```
Si `World` exige más métodos sin implementación por defecto, `cargo build` lo dirá: implementarlos devolviendo valores vacíos.

- [ ] **Step 5: `survival.rs` con tests (con un mundo simulado en memoria)**

Estructura (campos públicos al crate):

```rust
pub struct McSurvival {
    pub names: Names,
    pub inventory: Inventory,
    pub mining: Mining,
}
```
- `McSurvival::new(jar: &Path, catalog_json: &Path, names: Names) -> anyhow::Result<Self>`: `RecipeBook::from_jar(jar)?.with_item_catalog(Arc::new(ItemCatalog::from_path(catalog_json)?))`, `Inventory::default().with_recipes(book)`, `Mining::with_catalog(MiningCatalog::from_slice(&mining_catalog_json())?)` + `mining.set_loot(LootBook::from_jar(jar)?)`, `mining.set_loot_seed(0x5eed)`.
- `stack_to_c(&ItemStack) -> McStack`: `item = names.id(&stack.id).unwrap_or(0)`, `damage/max_damage` de `inventory.recipes.durability(stack)` (damage = `current`, max = `max_damage`; 0/0 sin durabilidad).
- `stack_from_c(item, count) -> Option<ItemStack>` con `ItemStack::new(name, count as u8)`.
- `mine(...) -> McMineResult`: construir `Player::new(eye_mc - (0.0, 1.62, 0.0))`, `yaw`, `pitch` de `yaw_pitch(dir)`, `on_ground`; `let mut w = CWorld{..}`; `let held = inventory.slots[sel].clone()`; `mining.tick(&mut w, &player, held.as_ref(), attacking)`; si devuelve `Some(broken)`: añadir cada drop con `inventory.add_item(stack, sel)` (sumar sobrantes a `lost`), luego `tool_broke = inventory.wear_tool_after_mining(sel, broken.hardness)`; rellenar `block_id = names.id(&broken.block.id)`. Siempre `progress = mining.progress()`.
- `save(path)` / `load(path)`: JSON `{"slots":[null|{"id","count","components"}...43]}`, escritura atómica (`path.tmp` + `rename`). `load` con archivo ausente, JSON roto o tamaño distinto de 43 deja el inventario vacío y devuelve `false`.
- `close()`: llamar `inventory.settle_crafting()`, `settle_workbench()` y `settle_cursor()`; reinsertar los stacks devueltos con `add_item(stack, 0)`; contar los que no caben.

Tests (los que se conocen deben existir y pasar; usan `MC_CLIENT_JAR` y `MC_ITEM_CATALOG` del entorno y se saltan con mensaje si faltan, nunca pasan en falso):
1. `mining_dirt_by_hand_takes_the_vanilla_time`: mundo de 3x3x3 de `dirt` en memoria; atacando sobre un bloque sin herramienta, `broken` ocurre al tick `ceil(1/(0.5/... ))`: dureza 0.5, velocidad 1, sin requisito: `progress = 1/0.5/30*ticks ≥ 1` → 15 ticks. Comprobar que no se rompe en el tick 14 y sí en el 15 y que `drops_added == 1` (`minecraft:dirt`).
2. `wooden_pickaxe_mines_stone_faster_than_hand`: con `wooden_pickaxe` en la ranura 0 se rompe `stone` antes que con la mano y con la mano no hay drop (herramienta requerida).
3. `unknown_or_air_cell_does_not_break`: celda con id 0 y con id 200: `broken == 0`, sin pánico.
4. `tool_wears_and_breaks`: pico de madera con `damage = max_damage - 1` rompe un bloque y `tool_broke == 1`, ranura 0 vacía.
5. `full_inventory_returns_leftover`: llenar las 36 ranuras con stacks de 64 distintos y `mc_inv_add` devuelve el sobrante > 0.
6. `crafting_logs_to_planks_to_table_to_pickaxe`: con `oak_log` en la rejilla 2x2 sale `oak_planks x4` (`mc_inv_take_output`), 4 tablones en la 2x2 dan `crafting_table`, 2 tablones en vertical dan 4 `stick`, y en la mesa 3x3 (3 tablones arriba + 2 palos) sale `wooden_pickaxe`.
7. `save_load_round_trip` y `load_missing_or_corrupt_starts_empty`.
8. `inventory_close_returns_grid_and_cursor`: items en la rejilla y en el cursor vuelven a las ranuras.

- [ ] **Step 6: API C en `lib.rs`.** Plantilla de guarda y de una función (repetir el patrón en todas):

```rust
use std::panic::{catch_unwind, AssertUnwindSafe};

fn guard<T>(default: T, f: impl FnOnce() -> T) -> T {
    catch_unwind(AssertUnwindSafe(f)).unwrap_or(default)
}

#[no_mangle]
pub unsafe extern "C" fn mc_inv_add(s: *mut McSurvival, item: i32, count: i32) -> i32 {
    guard(-1, || {
        let Some(s) = s.as_mut() else { return -1 };
        let Some(stack) = s.stack_from_c(item, count) else { return -1 };
        s.inventory.add_item(stack, 0).map_or(0, |left| left.count as i32)
    })
}
```
Cada puntero se comprueba con `as_ref()/as_mut()`; cada cadena C con `CStr::from_ptr(..).to_str()` (error → resultado de fallo); los índices con `get`. `mc_survival_create` guarda el motivo del fallo en un `static LAST_ERROR: Mutex<CString>` leído por `mc_survival_last_error`. Escribir `mc_bridge.h` con las firmas de la sección Interfaces.

- [ ] **Step 7: Ejecutar tests de Rust**

Run: `cd src/minecraft/rust/mc_bridge && MC_CLIENT_JAR="/mnt/1TB/Juegos/GTAs/0. GTA III/0. TEST/mcassets/client.jar" MC_ITEM_CATALOG="/mnt/1TB/Juegos/GTAs/0. GTA III/0. TEST/mcassets/item-catalog-26.3.json" cargo test -- --nocapture 2>&1 | tail -30`
Expected: todos PASS. Si el crafteo no devuelve lo esperado, depurar con systematic-debugging antes de seguir (las recetas salen del jar real).

- [ ] **Step 8: Commit**

```bash
git add src/minecraft/rust scripts/minecraft .gitignore
git commit -m "feat(minecraft): survival core in the rust bridge (inventory, mining, crafting, save)"
```

---

### Task 4: Integración jugable (hotbar, minar, colocar, parche F9)

**Files:**
- Create: `src/minecraft/game/McSurvival.h`, `src/minecraft/game/McSurvival.cpp`, `src/minecraft/core/McStarterPatch.h`, `src/minecraft/core/McStarterPatch.cpp`, `tests/minecraft/test_starterpatch.cpp`
- Modify: `src/minecraft/game/McMode.cpp`, `src/minecraft/game/McInteract.cpp`, `src/minecraft/game/McHotbar.cpp`, `src/minecraft/game/McInteract.h`

**Interfaces:**
- Consumes: API C de Task 3, `Mc::GetItemInfo`, `Mc::FindItemByName`, `Mc::World::{Get,Set}`.
- Produces:
  - `McSurvival::Init() -> bool` (carga jar, catálogo e inventario; `false` desactiva la supervivencia sin error fatal), `Shutdown()`, `bool IsReady()`, `int Count(int slot)`, `bool GetSlot(int area, int index, McStack &out)`, `int SelectedSlot()`, `void SetSelectedSlot(int)`, `void Mine(Mc::World&, eye, dir, bool attacking, bool onGround)`, `float MineProgress()`, `Save()`.
  - `Mc::BuildStarterPatch(World &w, int ox, int oy, int oz)` (esquina mínima del parche, 7x7 en planta).

- [ ] **Step 1: Test del parche (falla)** `tests/minecraft/test_starterpatch.cpp`: tras `BuildStarterPatch(w, 0, 0, 10)`, comprobar con un contador de ids sobre `[0,7)x[0,7)x[10,16)` que existen `BLOCK` ids de `oak_log` (≥4), `oak_leaves` (≥8), `stone` (≥9), `coal_ore` (≥2), `sand` y `gravel` (≥3 cada uno), y que ninguna celda por debajo de `oz` cambió (`world.Get(x,y,9) == 0`). Run: debe fallar (no existe).

- [ ] **Step 2: Implementar `McStarterPatch`.** Plataforma de 3x3 de piedra con una esquina de `coal_ore` (2 en total), un árbol de 4 troncos en el centro de otra zona con una copa de hojas 3x3x2 más una hoja encima, una banda de arena y otra de grava. Usa `FindItemByName` para obtener ids. Hasta que pase `mctests`.

- [ ] **Step 3: Envoltorio `McSurvival`.** Init: rutas `mcassets/client.jar` y `mcassets/item-catalog-26.3.json` (comprobar existencia con `FileSize` como hace `McAtlas`), nombres desde `Mc::GetItemInfo(i).name` para `i` en `0..ITEM_COUNT`, `mc_survival_create`; si falla imprimir `mc_survival_last_error()` y quedar no listo. Carga `mcsurvival.dat` con `mc_survival_load` (falta → inventario vacío). Si está listo y el inventario está completamente vacío, no se da nada: el jugador consigue sus items minando.

  Callbacks: `static int32_t GetCb(void *ctx, int32_t x, int32_t y, int32_t z) { return ((Mc::World*)ctx)->Get(x, y, z); }` y `SetCb` análogo con `Set(x, y, z, (uint8_t)id)`.

  Minería con acumulador de tiempo: `accumMs += CTimer::GetTimeStepInMilliseconds(); while(accumMs >= 50){ accumMs -= 50; mc_survival_mine(...) }` (comprobar el nombre exacto de la función en `Timer.h` y ajustar). Tras cada resultado con `broken`: imprimir bloque, drops y `tool_broke`; si `drops_lost > 0` imprimir aviso de inventario lleno.

- [ ] **Step 4: `McInteract` con supervivencia.** Con `McSurvival::IsReady()`:
  - Teclas `1..9` y rueda cambian `SelectedSlot` (9 ranuras), no `selected` de bloques.
  - Clic izquierdo mantenido (`pad->GetLeftMouse()`): si el rayo voxel es el más cercano, `McSurvival::Mine(...)` con `attacking = true`; clic soltado o apuntar a un peatón u otra cosa → `mc_survival_stop_mining`. El golpe a peatones conserva su comportamiento actual en `GetLeftMouseJustDown` cuando `gtaNearest`.
  - Clic derecho: lee la ranura seleccionada; si su id es bloque (`Mc::IsBlockItem`), coloca y llama `mc_inv_consume(slot, 1)`; si no es bloque o está vacía, no coloca (mensaje en el log).
  - Sin supervivencia (`!IsReady()`), el comportamiento es el actual (instantáneo, hotbar de bloques), para no regresar el modo clásico.

- [ ] **Step 5: Hotbar desde el inventario.** `McHotbar::Draw`: con supervivencia, 9 ranuras desde `McSurvival::GetSlot(0, i)`, icono del atlas con `BlockTileUV(item)`, cantidad abajo a la derecha con `CFont` (solo si `count > 1`), barra de durabilidad (fina, bajo el icono) si `max_damage > 0`: ancho proporcional a `1 - damage/max_damage`, verde→rojo; barra de progreso de minería bajo la mira mientras `MineProgress() > 0`. Nombre del item seleccionado bajo la barra como hoy.

- [ ] **Step 6: Tecla F9.** En `McMode::Update`, `if(active && pad->GetFJustDown(8)) BuildStarterPatch(world, floor(px)+3, floor(py)+3, floor(pz - 1.0f))` con la posición del jugador; log `McMode: starter patch built`. Colocar fuera del radio del cuerpo del jugador (offset 3). Guardado: `McSurvival::Save()` junto a `SaveWorld()` (autoguardado y salida).

- [ ] **Step 7: Compilar y probar a mano** (ejecutar según HANDOFF):
  1. F8, F9 → aparece el parche. Con la mano, mantener clic izquierdo sobre un tronco: barra de progreso, ~2 s hasta romper en vanilla (dureza 2, sin herramienta correcta: 100 ticks/… comprobar que es claramente más lento que piedra con pico) y aparece `oak_log` en la hotbar con cantidad.
  2. Romper piedra con la mano: no suelta nada (requiere pico).
  3. Colocar un bloque de la hotbar baja su cantidad y desaparece al llegar a 0.
  4. Salir y volver a entrar al juego: el inventario persiste.
  5. Con `mcassets/item-catalog-26.3.json` borrado: el juego arranca en modo clásico y lo dice en el log.

- [ ] **Step 8: Tests y commit**

Run: `cmake --build build/mctest -j && build/mctest/mctests` → PASS.
```bash
git add src/minecraft tests/minecraft
git commit -m "feat(minecraft): survival gameplay (mining, placing, hotbar from inventory, starter patch)"
```

---

### Task 5: Pantallas de inventario 2x2 y mesa 3x3

**Files:**
- Create: `src/minecraft/core/McInventoryLayout.h`, `src/minecraft/core/McInventoryLayout.cpp`, `tests/minecraft/test_inventorylayout.cpp`, `src/minecraft/game/McInventoryUI.h`, `src/minecraft/game/McInventoryUI.cpp`
- Modify: `src/minecraft/game/McMode.cpp` (abrir/cerrar, render), `src/minecraft/game/McInteract.cpp` (bloquear acciones con la UI abierta: ya hay `CanInteract`)

**Interfaces:**
- Produces (puro, sin re3):
  - `enum Mc::UiArea { AREA_INV = 0, AREA_GRID2 = 1, AREA_GRID3 = 2, AREA_CURSOR = 3, AREA_OUT2 = 4, AREA_OUT3 = 5, AREA_NONE = -1 };`
  - `struct Mc::UiSlot { int area; int index; float x, y, w, h; };`
  - `int Mc::BuildInventoryLayout(bool workbench, float screenW, float screenH, UiSlot *out, int max)` (devuelve nº de ranuras: 36 inventario + 4 armadura + rejilla + salida; 36+4+4+1 = 45 y con mesa 36+9+1 = 46).
  - `bool Mc::HitTestSlot(const UiSlot *slots, int n, float mx, float my, UiSlot &hit)`.

- [ ] **Step 1: Tests del layout (fallan).** `tests/minecraft/test_inventorylayout.cpp`:
  - `layout_counts`: sin mesa devuelve 45 ranuras, con mesa 46 (el panel de armadura solo existe en la pantalla E).
  - `layout_slots_do_not_overlap`: ningún par de rectángulos se solapa, en 1920x1080 y 2560x1440.
  - `layout_inside_screen_and_centred`: todas dentro de la pantalla; el panel queda centrado (márgenes izquierdo y derecho iguales con tolerancia 0.01).
  - `hit_test_finds_slot_centre_and_misses_gaps`: el centro de la ranura (AREA_INV, 0) devuelve esa ranura; un punto en el hueco entre dos ranuras devuelve `false`; fuera de la pantalla `false`; NaN devuelve `false`.
  - `hotbar_row_is_below_the_main_grid`: la fila 0..8 del inventario está por debajo de la 9..35 (convención de Minecraft).
  - Run: FAIL.

- [ ] **Step 2: Implementar `McInventoryLayout`.** Tamaño de ranura `screenH * 0.06f`, hueco `size * 0.1f`; panel centrado; filas de 9 con la hotbar separada con un hueco extra. Hasta que pase.

- [ ] **Step 3: `McInventoryUI`.**
  - Estado: `open`, `workbench` (bool), cursor del ratón propio `(mx, my)` que se integra con `CPad::GetPad(0)->GetMouseX()/GetMouseY()` (son incrementos) y se acota a la pantalla.
  - `Open(bool workbench)`: pone `CPad::GetPad(0)->DisablePlayerControls |= PLAYERCONTROL_CAMERA` (para que la cámara no gire) y guarda el valor anterior; `Close()` lo restaura y llama a `mc_inv_close` (log si se perdieron stacks).
  - Teclas: `E` (`GetCharJustDown('E')`) abre la pantalla 2x2 o cierra; `Esc` cierra. Clic derecho en una celda `crafting_table` a ≤ 5 de alcance (rayo voxel existente) abre la 3x3 en vez de colocar (se hace en `McInteract` antes de colocar).
  - Con la UI abierta: clic izquierdo/derecho (`GetLeftMouseJustDown/GetRightMouseJustDown`) sobre una ranura llama `mc_inv_click(area, index, right, shift)` o, en las ranuras de salida, `mc_inv_take_output(workbench, shift)`; `shift` desde `CPad::GetPad(0)->GetShift()` (comprobar el nombre real en `Pad.h`; si no existe, usar la tecla Shift de `NewKeyState`).
  - Dibujo (`Render2d`): fondo oscuro semitransparente a pantalla completa, rectángulos de ranura con el mismo estilo que la hotbar (icono desde el atlas, cantidad, barra de durabilidad), flecha entre rejilla y salida, el stack del cursor pegado al ratón, nombre del item bajo el ratón.
  - `McInteract::CanInteract()` no cambia; en `McMode::Update` si la UI está abierta no se llama a `McInteract::Update`.

- [ ] **Step 4: Compilar y probar a mano.**
  1. E abre; arrastrar con clic izquierdo mueve stacks; clic derecho reparte mitad/uno; shift-clic manda al otro bloque.
  2. 1 tronco en la rejilla → aparece salida 4 tablones; tomarla; 4 tablones en 2x2 → mesa de crafteo; 2 tablones en vertical → 4 palos.
  3. Colocar la mesa y clic derecho: se abre la 3x3; 3 tablones arriba y 2 palos en columna central → pico de madera.
  4. Pico de madera: romper piedra (suelta adoquín), la barra de durabilidad baja; con la durabilidad al límite se rompe y desaparece.
  5. Cerrar con items en la rejilla: vuelven al inventario. Esc y E cierran; la cámara vuelve a girar.
  6. Con la UI abierta no se rompe ni coloca nada, y peatones siguen sin ser golpeados.

- [ ] **Step 5: Tests y commit**

Run: `cmake --build build/mctest -j && build/mctest/mctests` → PASS.
```bash
git add src/minecraft tests/minecraft
git commit -m "feat(minecraft): 2x2 inventory and 3x3 crafting table screens"
```

---

### Task 6: Cierre de la fase

**Files:**
- Modify: `docs/minecraft/HANDOFF.md`, `CLAUDE.md` (sección Minecraft: una línea para el puente y `fetch-catalogs.sh`)

- [ ] **Step 1:** Actualizar `HANDOFF.md`: nuevas secciones de compilación (Rust, `RE3_MINECRAFT_SURVIVAL`, `RE3_MINECRAFTOSS_DIR`, script de catálogos), controles (E, F9, clic mantenido, 1-9), mapa de código nuevo, decisiones de este plan, limitaciones (sin horno, sin items en el suelo, mundo y inventario globales, sin antorcha) y pendientes renumerados.
- [ ] **Step 2:** Pasada completa: `cargo test` (con variables de entorno), `build/mctest/mctests`, build del juego sin errores nuevos de warnings en los ficheros tocados, recorrido manual de Task 4 y Task 5 de nuevo.
- [ ] **Step 3:** Revisión de rama completa con `superpowers:requesting-code-review`, correcciones, y commit:

```bash
git add docs CLAUDE.md
git commit -m "docs(minecraft): handoff for the survival core"
```
