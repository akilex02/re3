#include "McBlocks.h"
#include "McItemTable.h"

namespace Mc {

const BlockInfo &GetBlockInfo(uint8_t id)
{
	static BlockInfo table[BLOCK_COUNT];
	static bool built = false;
	if(!built){
		for(int i = 0; i < BLOCK_COUNT; i++){
			const ItemInfo &it = GetItemInfo((uint8_t)i);
			BlockInfo b = { it.name, it.r, it.g, it.b, it.transparent };
			table[i] = b;
		}
		built = true;
	}
	return table[id < BLOCK_COUNT ? id : (uint8_t)BLOCK_AIR];
}

}
