#include "McBlocks.h"

namespace Mc {

static const BlockInfo blockInfo[BLOCK_COUNT] = {
	{ "air",   0,   0,   0,   true  },
	{ "dirt",  134, 96,  67,  false },
	{ "stone", 125, 125, 125, false },
	{ "wood",  160, 130, 80,  false },
	{ "glass", 190, 225, 235, true  },
};

const BlockInfo &GetBlockInfo(uint8_t id)
{
	if(id >= BLOCK_COUNT)
		return blockInfo[BLOCK_AIR];
	return blockInfo[id];
}

}
