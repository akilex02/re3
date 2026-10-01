#pragma once

#ifdef MINECRAFT_MODE

#include "McSurvival.h"

// On-screen block hotbar. Called from McMode::Render2d during the 2D pass.
namespace McHotbar
{
	// Draws the slots, the selected frame and the selected block name.
	// Leaves the 2D pass render states as it found them and no texture raster bound.
	void Draw(void);
	// Icon, wear bar and stack count of one inventory slot in a rectangle (survival hotbar and inventory screens).
	// "minecraft:oak_planks" -> "oak planks"
	void DisplayName(const char *id, char *out, size_t size);
	void DrawSlotItem(const McSurv::Slot &slot, float x, float y, float w, float h);
}

#endif
