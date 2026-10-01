#include "mctest.h"
#include <string.h>
#include "McItemTable.h"
#include "McBlocks.h"

using namespace Mc;

MC_TEST(itemtable_legacy_ids_are_stable)
{
	MC_CHECK(strcmp(GetItemInfo(BLOCK_DIRT).name, "minecraft:dirt") == 0);
	MC_CHECK(strcmp(GetItemInfo(BLOCK_STONE).name, "minecraft:stone") == 0);
	MC_CHECK(strcmp(GetItemInfo(BLOCK_WOOD).name, "minecraft:oak_planks") == 0);
	MC_CHECK(strcmp(GetItemInfo(BLOCK_GLASS).name, "minecraft:glass") == 0);
	MC_CHECK_EQ(BLOCK_DIRT, 1);
	MC_CHECK_EQ(BLOCK_GLASS, 4);
}

MC_TEST(itemtable_names_are_unique_and_namespaced)
{
	for(int i = 1; i < ITEM_COUNT; i++){
		MC_CHECK(strncmp(GetItemInfo(i).name, "minecraft:", 10) == 0);
		MC_CHECK(GetItemInfo(i).texture != nullptr);
		for(int j = i + 1; j < ITEM_COUNT; j++)
			MC_CHECK(strcmp(GetItemInfo(i).name, GetItemInfo(j).name) != 0);
	}
}

MC_TEST(itemtable_blocks_come_first)
{
	MC_CHECK(IsBlockItem(1));
	MC_CHECK(IsBlockItem(LAST_BLOCK));
	MC_CHECK(!IsBlockItem(LAST_BLOCK + 1));
	MC_CHECK(!IsBlockItem(0));
	MC_CHECK_EQ(BLOCK_COUNT, LAST_BLOCK + 1);
}

MC_TEST(itemtable_fits_the_atlas)
{
	MC_CHECK(ITEM_COUNT <= 8 * 8);
}

MC_TEST(itemtable_find_by_name)
{
	MC_CHECK_EQ(FindItemByName("minecraft:dirt"), BLOCK_DIRT);
	MC_CHECK(FindItemByName("minecraft:stick") > LAST_BLOCK);
	MC_CHECK_EQ(FindItemByName("minecraft:nope"), -1);
	MC_CHECK_EQ(FindItemByName(nullptr), -1);
}

MC_TEST(itemtable_out_of_range_is_air)
{
	MC_CHECK(strcmp(GetItemInfo(ITEM_COUNT).name, "minecraft:air") == 0);
	MC_CHECK(strcmp(GetItemInfo(255).name, "minecraft:air") == 0);
}

MC_TEST(itemtable_basename)
{
	MC_CHECK(strcmp(TextureBasename("block/dirt.png"), "dirt.png") == 0);
	MC_CHECK(strcmp(TextureBasename("stick.png"), "stick.png") == 0);
}

// unzip -j flattens the jar directories, so two textures must not share a file name.
MC_TEST(itemtable_texture_basenames_are_unique)
{
	for(int i = 1; i < ITEM_COUNT; i++)
		for(int j = i + 1; j < ITEM_COUNT; j++)
			MC_CHECK(strcmp(TextureBasename(GetItemInfo(i).texture), TextureBasename(GetItemInfo(j).texture)) != 0);
}
