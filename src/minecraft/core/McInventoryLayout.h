#pragma once

namespace Mc {

// Inventory areas; the values match mc_bridge.h
enum UiArea {
	AREA_NONE = -1,
	AREA_INV = 0,	// 0..8 hotbar, 9..35 main, 36..39 armor (feet, legs, chest, head)
	AREA_GRID2 = 1,
	AREA_GRID3 = 2,
	AREA_CURSOR = 3,
	AREA_OUT2 = 4,
	AREA_OUT3 = 5
};

struct UiSlot {
	int area;
	int index;
	float x, y, w, h;	// top-left corner and size in screen pixels
};

// Fills `out` with the slots of the 2x2 inventory screen (workbench = false: 36 inventory + 4 armor + grid + output
// = 45 slots) or the 3x3 crafting table screen (workbench = true: 36 inventory + grid + output = 46 slots), centred
// on the screen. Returns the slot count, or 0 when the arguments are invalid or `max` is too small.
int BuildInventoryLayout(bool workbench, float screenW, float screenH, UiSlot *out, int max);

// The slot under the point, if any. Gaps between slots and invalid points give false.
bool HitTestSlot(const UiSlot *slots, int count, float x, float y, UiSlot &hit);

}
