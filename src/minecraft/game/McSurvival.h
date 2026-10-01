#pragma once

#include "McWorld.h"

// Survival state (inventory, mining progress) backed by the MinecraftOSS bridge. Without MINECRAFT_SURVIVAL
// every function is an inline stub that reports "not ready", so callers need no #ifdefs.
// The namespace is McSurv because the C bridge already owns the global name McSurvival (its opaque struct).
struct McSurvival;

namespace McSurv
{
	struct Slot {
		int item;	// Mc item id, 0 = empty
		int count;
		int damage;	// tool wear; maxDamage is 0 for items without durability
		int maxDamage;
	};

	// Inventory areas, as in mc_bridge.h
	enum Area { AREA_INV = 0, AREA_GRID2 = 1, AREA_GRID3 = 2, AREA_CURSOR = 3, AREA_OUT2 = 4, AREA_OUT3 = 5 };
	const int HOTBAR_SLOTS = 9;

#ifdef MINECRAFT_SURVIVAL
	// Every frame: creates the survival core once mcassets/client.jar and the item catalog exist. Never throws away
	// a failure for good: it retries when the files appear.
	void Update(void);
	// Saves the inventory and frees the core.
	void Shutdown(void);
	bool IsReady(void);
	bool GetSlot(int area, int index, Slot &out);
	int SelectedSlot(void);
	void SetSelectedSlot(int slot);
	// Removes one item from the given hotbar/inventory slot. False if the slot is empty.
	bool Consume(int slot);
	// One call per frame while the player may mine. `attacking` is the held button on a voxel target. Runs the
	// 20 Hz mining ticks that fit in the elapsed game time. Eye and direction are GTA world coordinates.
	void Mine(Mc::World &world, const double eye[3], const double dir[3], bool attacking, bool onGround);
	void StopMining(void);
	// 0..1 progress of the block being mined, 0 when idle.
	float MineProgress(void);
	bool Save(void);
	// Raw core for the inventory screens; nil unless IsReady().
	McSurvival *Core(void);
#else
	inline void Update(void) {}
	inline void Shutdown(void) {}
	inline bool IsReady(void) { return false; }
	inline bool GetSlot(int, int, Slot &) { return false; }
	inline int SelectedSlot(void) { return 0; }
	inline void SetSelectedSlot(int) {}
	inline bool Consume(int) { return false; }
	inline void Mine(Mc::World &, const double *, const double *, bool, bool) {}
	inline void StopMining(void) {}
	inline float MineProgress(void) { return 0.0f; }
	inline bool Save(void) { return false; }
#endif
}
