#pragma once

#ifdef MINECRAFT_MODE

// On-screen block hotbar. Called from McMode::Render2d during the 2D pass.
namespace McHotbar
{
	// Draws the slots, the selected frame and the selected block name.
	// Leaves the 2D pass render states as it found them and no texture raster bound.
	void Draw(void);
}

#endif
