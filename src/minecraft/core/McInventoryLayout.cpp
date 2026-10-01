#include <cmath>
#include "McInventoryLayout.h"

namespace Mc {

static const int COLUMNS = 9;
static const int TOP_ROWS = 4;	// armor, crafting grid and output sit above the main inventory
static const int MAIN_ROWS = 3;
static const float SLOT_SIZE = 0.06f;	// fraction of the screen height
static const float GAP = 0.1f;	// fraction of the slot size
static const float HOTBAR_GAP = 0.4f;	// extra space (in cells) between the main inventory and the hotbar

namespace {
	struct Builder {
		UiSlot *out;
		int max;
		int n;
		float size, cell, left, top;

		bool Add(int area, int index, float col, float row, float extraY)
		{
			if(n >= max)
				return false;
			UiSlot &s = out[n++];
			s.area = area;
			s.index = index;
			s.x = left + col * cell;
			s.y = top + row * cell + extraY * cell;
			s.w = size;
			s.h = size;
			return true;
		}
	};
}

int BuildInventoryLayout(bool workbench, float screenW, float screenH, UiSlot *out, int max)
{
	if(out == nullptr || !(screenW > 0.0f) || !(screenH > 0.0f) || max <= 0)
		return 0;
	Builder b;
	b.out = out;
	b.max = max;
	b.n = 0;
	b.size = screenH * SLOT_SIZE;
	b.cell = b.size * (1.0f + GAP);
	float rows = (float)(TOP_ROWS + MAIN_ROWS + 1);
	float width = (float)COLUMNS * b.cell - b.size * GAP;
	float height = rows * b.cell - b.size * GAP + HOTBAR_GAP * b.cell;
	b.left = (screenW - width) * 0.5f;
	b.top = (screenH - height) * 0.5f;
	if(width > screenW || height > screenH)
		return 0;

	bool ok = true;
	// main inventory (slots 9..35) and the hotbar below it
	for(int r = 0; r < MAIN_ROWS && ok; r++)
		for(int c = 0; c < COLUMNS && ok; c++)
			ok = b.Add(AREA_INV, 9 + r * COLUMNS + c, (float)c, (float)(TOP_ROWS + r), 0.0f);
	for(int c = 0; c < COLUMNS && ok; c++)
		ok = b.Add(AREA_INV, c, (float)c, (float)(TOP_ROWS + MAIN_ROWS), HOTBAR_GAP);

	if(workbench){
		for(int r = 0; r < 3 && ok; r++)
			for(int c = 0; c < 3 && ok; c++)
				ok = b.Add(AREA_GRID3, r * 3 + c, (float)(2 + c), (float)r + 0.5f, 0.0f);
		if(ok)
			ok = b.Add(AREA_OUT3, 0, 7.0f, 1.5f, 0.0f);
	}else{
		// armor from the head down: slots 39, 38, 37, 36
		for(int r = 0; r < 4 && ok; r++)
			ok = b.Add(AREA_INV, 39 - r, 0.0f, (float)r, 0.0f);
		for(int r = 0; r < 2 && ok; r++)
			for(int c = 0; c < 2 && ok; c++)
				ok = b.Add(AREA_GRID2, r * 2 + c, (float)(4 + c), (float)r + 1.0f, 0.0f);
		if(ok)
			ok = b.Add(AREA_OUT2, 0, 7.0f, 1.5f, 0.0f);
	}
	return ok ? b.n : 0;
}

bool HitTestSlot(const UiSlot *slots, int count, float x, float y, UiSlot &hit)
{
	if(slots == nullptr || count <= 0 || !std::isfinite(x) || !std::isfinite(y))
		return false;
	for(int i = 0; i < count; i++)
		if(x >= slots[i].x && x < slots[i].x + slots[i].w && y >= slots[i].y && y < slots[i].y + slots[i].h){
			hit = slots[i];
			return true;
		}
	return false;
}

}
