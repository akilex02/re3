#pragma once
#include <stdint.h>

namespace Mc {

enum BlockId : uint8_t {
	BLOCK_AIR = 0,
	BLOCK_DIRT,
	BLOCK_STONE,
	BLOCK_WOOD,
	BLOCK_GLASS,
	BLOCK_COUNT
};

struct BlockInfo {
	const char *name;
	uint8_t r, g, b;
	bool transparent;	// faces next to it stay visible; air is transparent
};

// Unknown ids return the air entry.
const BlockInfo &GetBlockInfo(uint8_t id);

}
