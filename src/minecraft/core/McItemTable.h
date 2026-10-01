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
