#pragma once

#ifdef MINECRAFT_MODE

// Survival inventory screens: E opens the 2x2 crafting inventory, a right click on a crafting table opens the 3x3.
// Without MINECRAFT_SURVIVAL every function is an inline stub.
namespace McInventoryUI
{
#ifdef MINECRAFT_SURVIVAL
	// Every frame while Steve mode is active: handles E, the mouse cursor and the slot clicks.
	void Update(void);
	bool IsOpen(void);
	// workbench = true shows the 3x3 grid. Does nothing unless the survival core is ready.
	void Open(bool workbench);
	// Returns the crafting grid and the held stack to the inventory and gives control back to the player.
	void Close(void);
	// Draws the screen during the 2D pass. Leaves the 2D pass render states as it found them.
	void Render2d(void);
#else
	inline void Update(void) {}
	inline bool IsOpen(void) { return false; }
	inline void Open(bool) {}
	inline void Close(void) {}
	inline void Render2d(void) {}
#endif
}

#endif
