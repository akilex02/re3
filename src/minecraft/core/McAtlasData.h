#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include "McBlocks.h"
#include "McItemTable.h"

namespace Mc {

const int ATLAS_TILES = 8;
const int TILE_PIXELS = 16;
const int ATLAS_PIXELS = ATLAS_TILES * TILE_PIXELS;

// Finds the first "key": "value" pair at or after `from`. Occurrences of the key that are not followed
// by a string value are skipped. Values containing a backslash are rejected.
bool JsonFindString(const std::string &json, const char *key, size_t from, std::string &value,
	size_t *keyPos = nullptr, size_t *after = nullptr);

bool ExtractLatestRelease(const std::string &manifest, std::string &id);
// The returned URL is NOT validated: callers must pass it through IsAllowedMojangUrl before using it.
bool ExtractVersionUrl(const std::string &manifest, const std::string &id, std::string &url);
// The returned URL is NOT validated: callers must pass it through IsAllowedMojangUrl before using it.
bool ExtractClientUrl(const std::string &versionJson, std::string &url);

// https only, allowlisted Mojang host, non-empty path, safe characters [A-Za-z0-9._~:/?=+-], < 512 chars, no '@'.
// Rejects % and & (unsafe on Windows cmd.exe), uppercase hosts, ports, trailing dots.
bool IsAllowedMojangUrl(const std::string &url);

// Path under assets/minecraft/textures/ for an item id (e.g. "block/dirt.png"), or nullptr.
const char *BlockTextureFile(uint8_t id);

struct TilePixels {
	bool valid;
	uint8_t rgba[TILE_PIXELS * TILE_PIXELS * 4];
};

// Composes 8x8 tiles (index = item id) into a 128x128 RGBA atlas, row 0 on top.
// Invalid tiles get the block's flat colour (magenta for ids >= ITEM_COUNT).
void ComposeAtlas(const TilePixels tiles[ATLAS_TILES * ATLAS_TILES], std::vector<uint8_t> &out);

}
