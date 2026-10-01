#pragma once
#include <stdint.h>

namespace Mc {

enum BlockId : uint8_t {
	BLOCK_AIR = 0,
	BLOCK_DIRT,
	BLOCK_STONE,
	BLOCK_WOOD,
	BLOCK_GLASS
};

// Placeable block ids are 1..BLOCK_COUNT-1 (see McItemTable.h).
const int BLOCK_COUNT = 30;
// The classic (non-survival) hotbar only offers ids 1..CLASSIC_BLOCKS.
const int CLASSIC_BLOCKS = 4;

struct BlockInfo {
	const char *name;
	uint8_t r, g, b;
	bool transparent;	// faces next to it stay visible; air is transparent
};

// Unknown ids return the air entry.
const BlockInfo &GetBlockInfo(uint8_t id);

}
