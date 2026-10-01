#include "McBlocks.h"
#include "McItemTable.h"

namespace Mc {

struct BlockTable {
	BlockInfo entries[BLOCK_COUNT];
	BlockTable()
	{
		for(int i = 0; i < BLOCK_COUNT; i++){
			const ItemInfo &it = GetItemInfo((uint8_t)i);
			BlockInfo b = { it.name, it.r, it.g, it.b, it.transparent };
			entries[i] = b;
		}
	}
};

const BlockInfo &GetBlockInfo(uint8_t id)
{
	static const BlockTable table;	// thread-safe initialisation
	return table.entries[id < BLOCK_COUNT ? id : (uint8_t)BLOCK_AIR];
}

}
