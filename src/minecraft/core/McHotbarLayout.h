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
