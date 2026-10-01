# Minecraft block textures from Mojang — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Blocks are drawn with the real Minecraft textures, fetched from Mojang at first launch, with flat colours as fallback.

**Architecture:** Pure, unit-tested parsing/composition code in the core (`McAtlasData`); a game-layer `McAtlas` that downloads in a detached background thread via `curl`/`unzip`, decodes PNGs with librw and builds a 64x64 atlas texture; `McRenderer` binds it and re-meshes with `textured = true` when it arrives.

**Tech Stack:** C++11, librw (`rw::readPNG`, `rw::Image`, `rw::Raster`, `rw::Texture` via the fakerw shim), `curl` and `unzip` command-line tools, `std::thread`/`std::atomic`.

**Spec:** `docs/superpowers/specs/2026-09-30-minecraft-textures-design.md` (parent: `2026-09-30-minecraft-mode-design.md`)

## Global Constraints

- C++11 only. Core (`src/minecraft/core/`) uses only the standard library and is unit-tested in `tests/minecraft/` (never put test files under `src/`).
- Game layer (`src/minecraft/game/`) follows `CODING_STYLE.md`: tabs, return type on its own line, brace on the next line for function definitions, no braces around single statements, `int *ptr`, project typedefs (`uint8`, `int32`), `nil`. Everything inside `#ifdef MINECRAFT_MODE`. The include order that compiles for ped/world headers is `Ped.h`, `PlayerPed.h`, then `PlayerInfo.h`, `World.h`.
- No Rockstar file is modified. Never stage `src/core/config.h` (the user's uncommitted reformatting) or `CLAUDE.md`; stage by explicit path. Never commit downloaded assets.
- Atlas layout: 4x4 tiles of 16x16 pixels = 64x64 RGBA; tile index = block id; tile `(id % 4, id / 4)`; tile row 0 is the TOP of the image (matches `McMesher` UVs, where `v` grows downward).
- Allowed hosts for downloads: `piston-meta.mojang.com`, `piston-data.mojang.com`, `launcher.mojang.com`, `launchermeta.mojang.com`. URLs reach a shell only after `IsAllowedMojangUrl`.
- `rw::readPNG` asserts when the file is missing: always check existence and non-zero size first.
- The downloader thread must never delay quitting the game (detached, process-lifetime state only, stop flag checked between steps, `curl --connect-timeout 10 --max-time 120`).
- Run tests: `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j && timeout 120 build/mctest/mctests` (from `/home/akilex/Descargas/gtas/re3`; currently 105 pass). Game build: `cd build && cmake . && cmake --build . -j$(nproc)`. Do not run the game in implementer tasks (the user playtests).
- Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

- A hostile or malformed manifest/URL must never reach the shell (Task 1, 2): host spoofing (`piston-data.mojang.com.evil.com`, `user@host`), shell metacharacters, overlong strings.
- Missing, empty or corrupt PNGs must not crash (`rw::readPNG` asserts on a missing file) (Task 2).
- Quitting the game while the download is running must not hang or crash (Task 2).
- Atlas orientation and tile order must match `McMesher` (tile row 0 on top, `(id%4, id/4)`) (Task 1, 3).
- Render state after drawing must still be restored exactly (Task 3).
- No network: flat colours, one log line, no retry loop (Task 2).

## File Structure

```
src/minecraft/core/McAtlasData.h/.cpp   JSON extraction, URL validation, file names, atlas composition
src/minecraft/core/McWorld.h/.cpp       + World::MarkAllDirty
src/minecraft/game/McAtlas.h/.cpp       cache, downloader thread, PNG loading, texture, generation
src/minecraft/game/McRenderer.cpp       binds atlas, textured meshing
src/minecraft/game/McMode.cpp           McAtlas Init/Update/Shutdown calls
tests/minecraft/test_atlasdata.cpp, test_world.cpp (MarkAllDirty)
.gitignore                              + mcassets/
```

---

### Task 1: `McAtlasData` and `World::MarkAllDirty` (core)

**Files:** Create `src/minecraft/core/McAtlasData.h`, `src/minecraft/core/McAtlasData.cpp`, `tests/minecraft/test_atlasdata.cpp`; Modify `src/minecraft/core/McWorld.h`, `src/minecraft/core/McWorld.cpp`, `tests/minecraft/test_world.cpp`.

**Interfaces (namespace `Mc`):**
- `const int ATLAS_TILES = 4; const int TILE_PIXELS = 16; const int ATLAS_PIXELS = 64;`
- `bool JsonFindString(const std::string &json, const char *key, size_t from, std::string &value, size_t *keyPos = nullptr, size_t *after = nullptr)`: finds the first occurrence of `"key"` at or after `from` that is followed (optional whitespace) by `:`, optional whitespace and a string value; returns it without quotes; `keyPos` = index of the key's opening quote; `after` = index just past the value's closing quote. Occurrences not followed by a string value are skipped. A backslash inside the value makes the function return false.
- `bool ExtractLatestRelease(const std::string &manifest, std::string &id)`: value of `release` found after `"latest"`.
- `bool ExtractVersionUrl(const std::string &manifest, const std::string &id, std::string &url)`: finds the manifest entry whose `"id"` equals `id` and returns the `"url"` of THAT entry (the url key must appear before the next `"id"` key; otherwise false).
- `bool ExtractClientUrl(const std::string &versionJson, std::string &url)`: `url` found after `"downloads"` then `"client"` (the exact key `client`, not `client_mappings`).
- `bool IsAllowedMojangUrl(const std::string &url)`: spec rules (https://, host exactly in the allowlist, `/` and a non-empty path after it, only `[A-Za-z0-9._~:/?&=%+-]` in the whole string, length < 512, no `@`).
- `const char *BlockTextureFile(uint8_t id)`: `dirt.png`, `stone.png`, `oak_planks.png`, `glass.png` for the four blocks; nullptr for air and unknown ids.
- `struct TilePixels { bool valid; uint8_t rgba[TILE_PIXELS * TILE_PIXELS * 4]; };` and `void ComposeAtlas(const TilePixels tiles[ATLAS_TILES * ATLAS_TILES], std::vector<uint8_t> &out)`: `out` becomes 64*64*4 RGBA bytes; valid tiles are copied into place; an invalid tile is filled with that block id's flat colour (`GetBlockInfo(id)`, alpha 255) for `id < BLOCK_COUNT`, and magenta `(255,0,255,255)` otherwise.
- `void World::MarkAllDirty()`: sets `dirty = true` on every chunk (no change to `Revision()`).

- [ ] **Step 1: Write the failing tests**

`tests/minecraft/test_atlasdata.cpp`:
```cpp
#include "mctest.h"
#include "McAtlasData.h"
#include <string.h>

using namespace Mc;

static const char *kManifest =
	"{\"latest\":{\"release\":\"1.21.4\",\"snapshot\":\"25w01a\"},\"versions\":["
	"{\"id\":\"25w01a\",\"type\":\"snapshot\",\"url\":\"https://piston-meta.mojang.com/v1/packages/aaa/25w01a.json\",\"time\":\"t\"},"
	"{\"id\": \"1.21.4\", \"type\": \"release\", \"url\": \"https://piston-meta.mojang.com/v1/packages/bbb/1.21.4.json\", \"time\": \"t\"}]}";

static const char *kVersion =
	"{\"downloads\":{\"client\":{\"sha1\":\"x\",\"size\":1,\"url\":\"https://piston-data.mojang.com/v1/objects/abc/client.jar\"},"
	"\"client_mappings\":{\"url\":\"https://piston-data.mojang.com/v1/objects/zzz/client.txt\"},"
	"\"server\":{\"url\":\"https://piston-data.mojang.com/v1/objects/s/server.jar\"}}}";

MC_TEST(atlas_json_find_string_basic_and_whitespace)
{
	std::string v;
	size_t kp = 0, af = 0;
	MC_CHECK(JsonFindString("{ \"a\" :  \"hello\" }", "a", 0, v, &kp, &af));
	MC_CHECK(v == "hello");
	MC_CHECK_EQ(kp, 2);
	MC_CHECK(!JsonFindString("{\"a\":\"x\"}", "b", 0, v));
}

MC_TEST(atlas_json_find_string_skips_non_string_values)
{
	std::string v;
	MC_CHECK(JsonFindString("{\"k\":{\"x\":1},\"k\":\"second\"}", "k", 0, v));
	MC_CHECK(v == "second");
	MC_CHECK(!JsonFindString("{\"k\":5}", "k", 0, v));
}

MC_TEST(atlas_json_find_string_rejects_backslash_values)
{
	std::string v;
	MC_CHECK(!JsonFindString("{\"k\":\"a\\\"b\"}", "k", 0, v));
}

MC_TEST(atlas_latest_release)
{
	std::string id;
	MC_CHECK(ExtractLatestRelease(kManifest, id));
	MC_CHECK(id == "1.21.4");
	MC_CHECK(!ExtractLatestRelease("{\"versions\":[]}", id));
}

MC_TEST(atlas_version_url_picks_the_matching_entry)
{
	std::string url;
	MC_CHECK(ExtractVersionUrl(kManifest, "1.21.4", url));
	MC_CHECK(url == "https://piston-meta.mojang.com/v1/packages/bbb/1.21.4.json");
	MC_CHECK(ExtractVersionUrl(kManifest, "25w01a", url));
	MC_CHECK(url == "https://piston-meta.mojang.com/v1/packages/aaa/25w01a.json");
	MC_CHECK(!ExtractVersionUrl(kManifest, "9.9", url));
}

MC_TEST(atlas_version_url_does_not_borrow_the_next_entries_url)
{
	std::string url;
	// the entry for "a" has no url; the next entry "b" has one
	const char *m = "{\"versions\":[{\"id\":\"a\",\"type\":\"release\"},{\"id\":\"b\",\"url\":\"https://piston-meta.mojang.com/x.json\"}]}";
	MC_CHECK(!ExtractVersionUrl(m, "a", url));
	MC_CHECK(ExtractVersionUrl(m, "b", url));
}

MC_TEST(atlas_client_url_is_not_the_mappings_url)
{
	std::string url;
	MC_CHECK(ExtractClientUrl(kVersion, url));
	MC_CHECK(url == "https://piston-data.mojang.com/v1/objects/abc/client.jar");
	MC_CHECK(!ExtractClientUrl("{\"downloads\":{\"client_mappings\":{\"url\":\"https://piston-data.mojang.com/z\"}}}", url));
	MC_CHECK(!ExtractClientUrl("{}", url));
}

MC_TEST(atlas_allowed_urls)
{
	MC_CHECK(IsAllowedMojangUrl("https://piston-data.mojang.com/v1/objects/abc/client.jar"));
	MC_CHECK(IsAllowedMojangUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"));
	MC_CHECK(IsAllowedMojangUrl("https://launcher.mojang.com/v1/objects/a/b.jar"));
	MC_CHECK(IsAllowedMojangUrl("https://launchermeta.mojang.com/mc/game/x.json"));
}

MC_TEST(atlas_rejected_urls)
{
	MC_CHECK(!IsAllowedMojangUrl(""));
	MC_CHECK(!IsAllowedMojangUrl("http://piston-data.mojang.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://example.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com.evil.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://evil.com/piston-data.mojang.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://user@piston-data.mojang.com/x"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a b"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a;rm -rf"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a\"b"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a`b`"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a$(x)"));
	MC_CHECK(!IsAllowedMojangUrl("https://piston-data.mojang.com/a\nb"));
	MC_CHECK(!IsAllowedMojangUrl(std::string("https://piston-data.mojang.com/") + std::string(600, 'a')));
}

MC_TEST(atlas_block_texture_files)
{
	MC_CHECK(BlockTextureFile(BLOCK_AIR) == nullptr);
	MC_CHECK(strcmp(BlockTextureFile(BLOCK_DIRT), "dirt.png") == 0);
	MC_CHECK(strcmp(BlockTextureFile(BLOCK_STONE), "stone.png") == 0);
	MC_CHECK(strcmp(BlockTextureFile(BLOCK_WOOD), "oak_planks.png") == 0);
	MC_CHECK(strcmp(BlockTextureFile(BLOCK_GLASS), "glass.png") == 0);
	MC_CHECK(BlockTextureFile(200) == nullptr);
}

static void FillTile(TilePixels &t, uint8_t v)
{
	t.valid = true;
	for(int i = 0; i < TILE_PIXELS * TILE_PIXELS * 4; i++)
		t.rgba[i] = v;
}

MC_TEST(atlas_compose_places_tiles_and_falls_back_to_flat_colours)
{
	TilePixels tiles[ATLAS_TILES * ATLAS_TILES];
	for(int i = 0; i < ATLAS_TILES * ATLAS_TILES; i++){
		tiles[i].valid = false;
		memset(tiles[i].rgba, 0, sizeof(tiles[i].rgba));
	}
	FillTile(tiles[1], 77);			// dirt tile valid, all bytes 77
	// mark one distinctive pixel in tile 5 (tx=1, ty=1): local (3,2) = 1,2,3,4
	tiles[5].valid = true;
	uint8_t *p = &tiles[5].rgba[(2 * TILE_PIXELS + 3) * 4];
	p[0] = 1; p[1] = 2; p[2] = 3; p[3] = 4;

	std::vector<uint8_t> out;
	ComposeAtlas(tiles, out);
	MC_CHECK_EQ(out.size(), ATLAS_PIXELS * ATLAS_PIXELS * 4);

	// tile 1 = (tx=1, ty=0): pixel (16,0)
	MC_CHECK_EQ(out[(0 * ATLAS_PIXELS + 16) * 4], 77);
	// tile 5 distinctive pixel at atlas (16+3, 16+2)
	const uint8_t *q = &out[((16 + 2) * ATLAS_PIXELS + (16 + 3)) * 4];
	MC_CHECK_EQ(q[0], 1); MC_CHECK_EQ(q[1], 2); MC_CHECK_EQ(q[2], 3); MC_CHECK_EQ(q[3], 4);
	// tile 2 (stone, tx=2, ty=0) is invalid: flat colour of stone, alpha 255
	const uint8_t *s = &out[(0 * ATLAS_PIXELS + 32) * 4];
	MC_CHECK_EQ(s[0], GetBlockInfo(BLOCK_STONE).r);
	MC_CHECK_EQ(s[1], GetBlockInfo(BLOCK_STONE).g);
	MC_CHECK_EQ(s[2], GetBlockInfo(BLOCK_STONE).b);
	MC_CHECK_EQ(s[3], 255);
	// tile 15 (id >= BLOCK_COUNT) is invalid: magenta
	const uint8_t *m = &out[((3 * 16) * ATLAS_PIXELS + 3 * 16) * 4];
	MC_CHECK_EQ(m[0], 255); MC_CHECK_EQ(m[1], 0); MC_CHECK_EQ(m[2], 255); MC_CHECK_EQ(m[3], 255);
}
```
Append to `tests/minecraft/test_world.cpp`:
```cpp
MC_TEST(world_mark_all_dirty_sets_every_chunk_without_changing_revision)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);
	w.Set(40, 0, 0, BLOCK_DIRT);
	for(World::ChunkMap::const_iterator it = w.Chunks().begin(); it != w.Chunks().end(); ++it)
		it->second->dirty = false;
	uint32_t rev = w.Revision();
	w.MarkAllDirty();
	for(World::ChunkMap::const_iterator it = w.Chunks().begin(); it != w.Chunks().end(); ++it)
		MC_CHECK(it->second->dirty);
	MC_CHECK_EQ(w.Revision(), rev);
}
```
- [ ] **Step 2: Run to verify it fails** — `cmake -S tests/minecraft -B build/mctest && cmake --build build/mctest -j` → compile errors (`McAtlasData.h` missing, `MarkAllDirty` missing).
- [ ] **Step 3: Implement**

`src/minecraft/core/McAtlasData.h`:
```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include "McBlocks.h"

namespace Mc {

const int ATLAS_TILES = 4;
const int TILE_PIXELS = 16;
const int ATLAS_PIXELS = ATLAS_TILES * TILE_PIXELS;

// Finds the first "key": "value" pair at or after `from`. Occurrences of the key that are not followed
// by a string value are skipped. Values containing a backslash are rejected.
bool JsonFindString(const std::string &json, const char *key, size_t from, std::string &value,
	size_t *keyPos = nullptr, size_t *after = nullptr);

bool ExtractLatestRelease(const std::string &manifest, std::string &id);
bool ExtractVersionUrl(const std::string &manifest, const std::string &id, std::string &url);
bool ExtractClientUrl(const std::string &versionJson, std::string &url);

// https only, allowlisted Mojang host, non-empty path, safe characters, < 512 chars, no '@'.
bool IsAllowedMojangUrl(const std::string &url);

// File name under assets/minecraft/textures/block/ for a block id, or nullptr.
const char *BlockTextureFile(uint8_t id);

struct TilePixels {
	bool valid;
	uint8_t rgba[TILE_PIXELS * TILE_PIXELS * 4];
};

// Composes 4x4 tiles (index = block id) into a 64x64 RGBA atlas, row 0 on top.
// Invalid tiles get the block's flat colour (magenta for ids >= BLOCK_COUNT).
void ComposeAtlas(const TilePixels tiles[ATLAS_TILES * ATLAS_TILES], std::vector<uint8_t> &out);

}
```

`src/minecraft/core/McAtlasData.cpp`:
```cpp
#include "McAtlasData.h"
#include <string.h>

namespace Mc {

bool JsonFindString(const std::string &json, const char *key, size_t from, std::string &value,
	size_t *keyPos, size_t *after)
{
	std::string needle = std::string("\"") + key + "\"";
	size_t pos = from;
	for(;;){
		pos = json.find(needle, pos);
		if(pos == std::string::npos)
			return false;
		size_t i = pos + needle.size();
		while(i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n'))
			i++;
		if(i < json.size() && json[i] == ':'){
			i++;
			while(i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n'))
				i++;
			if(i < json.size() && json[i] == '"'){
				size_t end = i + 1;
				while(end < json.size() && json[end] != '"'){
					if(json[end] == '\\')
						return false;
					end++;
				}
				if(end >= json.size())
					return false;
				value = json.substr(i + 1, end - i - 1);
				if(keyPos) *keyPos = pos;
				if(after) *after = end + 1;
				return true;
			}
		}
		pos += needle.size();
	}
}

bool ExtractLatestRelease(const std::string &manifest, std::string &id)
{
	size_t l = manifest.find("\"latest\"");
	if(l == std::string::npos)
		return false;
	return JsonFindString(manifest, "release", l, id);
}

bool ExtractVersionUrl(const std::string &manifest, const std::string &id, std::string &url)
{
	size_t pos = 0;
	for(;;){
		std::string v;
		size_t after = 0;
		if(!JsonFindString(manifest, "id", pos, v, nullptr, &after))
			return false;
		if(v == id){
			size_t keyPos = 0;
			if(!JsonFindString(manifest, "url", after, url, &keyPos))
				return false;
			size_t nextId = manifest.find("\"id\"", after);
			if(nextId != std::string::npos && nextId < keyPos)
				return false;	// that url belongs to the next entry
			return true;
		}
		pos = after;
	}
}

bool ExtractClientUrl(const std::string &versionJson, std::string &url)
{
	size_t d = versionJson.find("\"downloads\"");
	if(d == std::string::npos)
		return false;
	size_t c = versionJson.find("\"client\"", d);
	if(c == std::string::npos)
		return false;
	return JsonFindString(versionJson, "url", c, url);
}

bool IsAllowedMojangUrl(const std::string &url)
{
	static const char *prefix = "https://";
	static const char *hosts[] = {
		"piston-meta.mojang.com", "piston-data.mojang.com", "launcher.mojang.com", "launchermeta.mojang.com"
	};
	if(url.size() >= 512 || url.compare(0, strlen(prefix), prefix) != 0)
		return false;
	for(size_t i = 0; i < url.size(); i++){
		char c = url[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			c == '.' || c == '_' || c == '~' || c == ':' || c == '/' || c == '?' || c == '&' ||
			c == '=' || c == '%' || c == '+' || c == '-';
		if(!ok)
			return false;
	}
	size_t hostStart = strlen(prefix);
	size_t slash = url.find('/', hostStart);
	if(slash == std::string::npos || slash + 1 >= url.size())
		return false;
	std::string host = url.substr(hostStart, slash - hostStart);
	for(size_t i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++)
		if(host == hosts[i])
			return true;
	return false;
}

const char *BlockTextureFile(uint8_t id)
{
	switch(id){
	case BLOCK_DIRT: return "dirt.png";
	case BLOCK_STONE: return "stone.png";
	case BLOCK_WOOD: return "oak_planks.png";
	case BLOCK_GLASS: return "glass.png";
	default: return nullptr;
	}
}

void ComposeAtlas(const TilePixels tiles[ATLAS_TILES * ATLAS_TILES], std::vector<uint8_t> &out)
{
	out.assign(ATLAS_PIXELS * ATLAS_PIXELS * 4, 0);
	for(int t = 0; t < ATLAS_TILES * ATLAS_TILES; t++){
		int tx = t % ATLAS_TILES, ty = t / ATLAS_TILES;
		for(int y = 0; y < TILE_PIXELS; y++)
		for(int x = 0; x < TILE_PIXELS; x++){
			uint8_t *dst = &out[((ty * TILE_PIXELS + y) * ATLAS_PIXELS + tx * TILE_PIXELS + x) * 4];
			if(tiles[t].valid){
				memcpy(dst, &tiles[t].rgba[(y * TILE_PIXELS + x) * 4], 4);
			}else if(t < BLOCK_COUNT){
				const BlockInfo &bi = GetBlockInfo((uint8_t)t);
				dst[0] = bi.r; dst[1] = bi.g; dst[2] = bi.b; dst[3] = 255;
			}else{
				dst[0] = 255; dst[1] = 0; dst[2] = 255; dst[3] = 255;
			}
		}
	}
}

}
```
`World::MarkAllDirty` — in `McWorld.h` (public): `void MarkAllDirty();` and in `McWorld.cpp`:
```cpp
void World::MarkAllDirty()
{
	for(ChunkMap::iterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
		it->second->dirty = true;
}
```
- [ ] **Step 4: Run to verify it passes** — all pass, no `-Wall -Wextra` warnings. If an expectation seems wrong, report it with the arithmetic; do not weaken tests.
- [ ] **Step 5: Commit** — stage the new/changed files by explicit path; subject `feat(minecraft): atlas data parsing and composition`.

---

### Task 2: `McAtlas` (downloader thread, PNG loading, texture)

**Files:** Create `src/minecraft/game/McAtlas.h`, `src/minecraft/game/McAtlas.cpp`; Modify `.gitignore`.

**Interfaces:**
- Consumes: `Mc::JsonFindString`/`ExtractLatestRelease`/`ExtractVersionUrl`/`ExtractClientUrl`/`IsAllowedMojangUrl`/`BlockTextureFile`/`TilePixels`/`ComposeAtlas`, librw/fakerw image, raster and texture functions.
- Produces (namespace `McAtlas`): `void Init(void)` (called once from `McMode::Init`: if all four PNGs exist in `mcassets/` load them immediately; otherwise start the detached download thread), `void Update(void)` (every frame on the main thread: when the thread reports success, load the PNGs and build the texture), `RwTexture *GetTexture(void)` (nil until ready), `uint32 Generation(void)` (starts at 0; incremented each time a new texture becomes available), `void Shutdown(void)` (sets the stop flag, never blocks, releases the texture).

Behaviour:
1. Cache check: `mcassets/<file>` exists and is non-empty for every `BlockTextureFile(id)` of the four blocks.
2. Download thread (`std::thread`, detached; shared state only in file-scope `std::atomic` variables and no heap objects with non-trivial destructors touched after main exits): create `mcassets/` (`mkdir`; on Windows `_mkdir`, behind `#ifdef _WIN32`), then with `system()` and these commands (build them with the validated URL; quote with double quotes):
   - `curl -fsSL --connect-timeout 10 --max-time 60 -o "mcassets/manifest.json" "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"`
   - read the file into a `std::string`; `ExtractLatestRelease`; `ExtractVersionUrl`; check `IsAllowedMojangUrl`; `curl ... -o "mcassets/version.json" "<url>"`; read; `ExtractClientUrl`; check `IsAllowedMojangUrl`.
   - `curl -fsSL --connect-timeout 10 --max-time 120 -o "mcassets/client.jar" "<url>"`.
   - `unzip -o -j -q "mcassets/client.jar" "assets/minecraft/textures/block/dirt.png" "assets/minecraft/textures/block/stone.png" "assets/minecraft/textures/block/oak_planks.png" "assets/minecraft/textures/block/glass.png" -d "mcassets"` (generate the list from `BlockTextureFile`).
   - delete `client.jar`, `manifest.json`, `version.json` (`remove`).
   - Check the stop flag between steps and abort quietly if set. Print one line per step (`McAtlas: ...`) and one clear line on failure (which step, e.g. `McAtlas: download failed at step 'client jar' (curl returned 6); using flat colours`). Success sets an atomic `ready` flag. The thread runs at most once per launch.
3. Loading (main thread): for each of the four blocks: check the file exists and its size is > 0 and < 1 MB (a block texture is tiny), then `rw::readPNG`/`RwImageRead`-equivalent; require width and height of at least 16 (animated textures such as water are taller strips; for the four used blocks the image is 16x16: if the image is larger take the top-left 16x16 and say so once in the log), convert to 32-bit RGBA when it is 24-bit or paletted, copy 16x16 RGBA into `Mc::TilePixels` (`valid = true`), otherwise leave the tile invalid and log which file failed and why. `ComposeAtlas` -> 64x64 RGBA buffer -> `rw::Image` (32 bit) -> `Raster::createFromImage` -> `Texture` with nearest filtering and clamp addressing, no mipmaps. If no tile was valid, do not create a texture (flat colours stay). On success increment the generation counter and log `McAtlas: atlas ready (N of 4 textures)`.
4. Never call librw from the thread. Never touch rw objects before `Init`. `Shutdown` must not wait for the thread.
5. `.gitignore`: add a line `mcassets/` (and nothing else).

- [ ] **Step 1: Verify the APIs** (grep and record the real names): `rw::readPNG` declaration/header and how re3 code can call it (namespace, include `rw.h` via the fakerw headers); `rw::Image::create/allocate/pixels/stride/width/height/depth/palette` and a conversion to 32-bit (`convertTo32`/`setPixels`), `rw::Raster::createFromImage` (and `RwRasterCreate`/`RwRasterSetFromImage` in `src/fakerw/rwcore.h`), `RwTextureCreate`, `RwTextureSetFilterMode(rwFILTERNEAREST)`, `RwTextureSetAddressing(rwTEXTUREADDRESSCLAMP)`, `RwTextureDestroy`; how an existing re3 file creates a texture from memory/image (grep `RwTextureCreate` in src). Check the pixel byte order librw expects for a 32-bit `Image` (RGBA) and that `createFromImage` gives a raster that `rwRENDERSTATETEXTURERASTER` can bind.
- [ ] **Step 2: Implement** `McAtlas.h` (the interface above) and `McAtlas.cpp`. Keep the command strings built from constants plus validated URLs only. All of it inside `#ifdef MINECRAFT_MODE`. Use `fopen/fseek/ftell` to size-check files before decoding.
- [ ] **Step 3: Build** — `cd build && cmake . && cmake --build . -j$(nproc)`: no `error`, final `Linking CXX executable src/re3`, no new warnings from `src/minecraft`; standalone tests still pass. McAtlas is not wired yet (Task 3), so nothing changes at runtime.
- [ ] **Step 4: Commit** — `git add src/minecraft/game/McAtlas.h src/minecraft/game/McAtlas.cpp .gitignore`; subject `feat(minecraft): download and load Mojang block textures`.

---

### Task 3: Renderer and mode integration

**Files:** Modify `src/minecraft/game/McRenderer.cpp`, `src/minecraft/game/McMode.cpp`.

**Interfaces:** Consumes `McAtlas::{Init,Update,GetTexture,Generation,Shutdown}`, `World::MarkAllDirty`, `Mc::MeshChunk(w, cp, textured, out)`.

Behaviour:
- `McMode::Init` calls `McAtlas::Init()`; `McMode::Update` calls `McAtlas::Update()` once per frame (outside the `if(active)` blocks, before the entities update); `McMode::Shutdown` calls `McAtlas::Shutdown()` before `McRenderer::Shutdown()`.
- `McRenderer::Render`: remember the last seen `McAtlas::Generation()`; when it changes, call `world.MarkAllDirty()` and clear the cached meshes so every chunk is re-meshed with the new `textured` value (`textured = (McAtlas::GetTexture() != nil)`). When a texture exists, set `rwRENDERSTATETEXTURERASTER` to its raster before drawing (instead of `nil`); with no texture keep today's behaviour. Keep the existing save/restore of ALL render states exactly as it is (the raster state is already saved and restored).
- Glass: keep the vertex alpha 140 for transparent blocks; the texture's own alpha multiplies (verify the vertex alpha test/blend states still make transparent texels non-writing for z if the existing code does that; do not add new state beyond what is needed and note any decision).

- [ ] **Step 1: Implement** the changes above.
- [ ] **Step 2: Build** — same commands; no errors, no new warnings from `src/minecraft`; standalone tests all pass.
- [ ] **Step 3: Commit** — stage the two files; subject `feat(minecraft): draw blocks with Mojang textures`.
- Manual in-game checks (user, later): first launch prints the McAtlas steps and the blocks change from flat colours to Minecraft textures without a restart; second launch uses `mcassets/` and shows textures at once; with the network off the first launch keeps flat colours and logs the reason; deleting `mcassets/` re-downloads; quitting while the download runs does not hang; shadows and effects still render next to blocks.

---

## Self-Review

**Spec coverage:** flow and cache (Task 2), atlas layout and composition (Task 1), re-mesh on arrival and `textured` meshing (Task 3), failure handling and URL validation (Tasks 1-2), shutdown safety (Task 2), `.gitignore` (Task 2). Out of scope exactly as in the spec.

**Type consistency:** `TilePixels`, `ComposeAtlas`, `BlockTextureFile`, `IsAllowedMojangUrl` and `World::MarkAllDirty` are defined in Task 1 and consumed in Tasks 2-3 with the same names. `McAtlas::Generation()` is `uint32` in both its definition and use.

**Known risks for the executor:** (1) librw's `readPNG` asserts on a missing file and returns nil on a corrupt one: guard both; (2) the exact librw API for creating a raster from a 32-bit image differs between librw versions: use what Step 1 finds; (3) the detached thread must not touch anything destroyed at exit.
