#include "McHotbarLayout.h"
#include "McAtlasData.h"

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
	const float inset = 0.5f / (float)ATLAS_PIXELS;
	int tx = id % ATLAS_TILES, ty = id / ATLAS_TILES;
	u0 = (float)tx / (float)ATLAS_TILES + inset;
	u1 = (float)(tx + 1) / (float)ATLAS_TILES - inset;
	v0 = (float)ty / (float)ATLAS_TILES + inset;
	v1 = (float)(ty + 1) / (float)ATLAS_TILES - inset;
}

int HotbarCount()
{
	return CLASSIC_BLOCKS;
}

uint8_t HotbarBlock(int index)
{
	if(index < 0 || index >= HotbarCount())
		return BLOCK_AIR;
	return (uint8_t)(index + 1);
}

int HotbarIndexOfBlock(uint8_t id)
{
	if(id < 1 || id > CLASSIC_BLOCKS)
		return -1;
	return id - 1;
}

}
