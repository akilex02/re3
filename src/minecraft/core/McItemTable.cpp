#include <string.h>
#include "McItemTable.h"

namespace Mc {

static const ItemInfo items[ITEM_COUNT] = {
	{ "minecraft:air", nullptr, 0, 0, 0, true },
	// blocks (1..29); 1..4 keep their historical ids
	{ "minecraft:dirt", "block/dirt.png", 134, 96, 67, false },
	{ "minecraft:stone", "block/stone.png", 125, 125, 125, false },
	{ "minecraft:oak_planks", "block/oak_planks.png", 160, 130, 80, false },
	{ "minecraft:glass", "block/glass.png", 190, 225, 235, true },
	{ "minecraft:grass_block", "block/grass_block_side.png", 95, 159, 53, false },
	{ "minecraft:cobblestone", "block/cobblestone.png", 110, 110, 110, false },
	{ "minecraft:sand", "block/sand.png", 219, 207, 163, false },
	{ "minecraft:gravel", "block/gravel.png", 131, 127, 126, false },
	{ "minecraft:oak_log", "block/oak_log.png", 102, 81, 50, false },
	{ "minecraft:oak_leaves", "block/oak_leaves.png", 60, 130, 40, true },
	{ "minecraft:coal_ore", "block/coal_ore.png", 105, 105, 105, false },
	{ "minecraft:iron_ore", "block/iron_ore.png", 136, 115, 100, false },
	{ "minecraft:crafting_table", "block/crafting_table_front.png", 143, 100, 55, false },
	{ "minecraft:furnace", "block/furnace_front.png", 120, 120, 120, false },
	{ "minecraft:birch_log", "block/birch_log.png", 216, 215, 210, false },
	{ "minecraft:birch_planks", "block/birch_planks.png", 192, 175, 121, false },
	{ "minecraft:spruce_log", "block/spruce_log.png", 58, 37, 16, false },
	{ "minecraft:spruce_planks", "block/spruce_planks.png", 114, 84, 48, false },
	{ "minecraft:bricks", "block/bricks.png", 150, 97, 83, false },
	{ "minecraft:sandstone", "block/sandstone.png", 216, 203, 155, false },
	{ "minecraft:stone_bricks", "block/stone_bricks.png", 122, 122, 122, false },
	{ "minecraft:coal_block", "block/coal_block.png", 16, 16, 16, false },
	{ "minecraft:iron_block", "block/iron_block.png", 220, 220, 220, false },
	{ "minecraft:diamond_ore", "block/diamond_ore.png", 120, 150, 150, false },
	{ "minecraft:diamond_block", "block/diamond_block.png", 98, 237, 228, false },
	{ "minecraft:gold_ore", "block/gold_ore.png", 145, 135, 100, false },
	{ "minecraft:gold_block", "block/gold_block.png", 246, 208, 61, false },
	{ "minecraft:white_wool", "block/white_wool.png", 233, 236, 236, false },
	{ "minecraft:obsidian", "block/obsidian.png", 20, 18, 30, false },
	// items (30..57)
	{ "minecraft:stick", "item/stick.png", 130, 100, 50, false },
	{ "minecraft:coal", "item/coal.png", 30, 30, 30, false },
	{ "minecraft:iron_ingot", "item/iron_ingot.png", 216, 216, 216, false },
	{ "minecraft:raw_iron", "item/raw_iron.png", 200, 160, 130, false },
	{ "minecraft:diamond", "item/diamond.png", 90, 230, 220, false },
	{ "minecraft:gold_ingot", "item/gold_ingot.png", 246, 208, 61, false },
	{ "minecraft:raw_gold", "item/raw_gold.png", 220, 180, 60, false },
	{ "minecraft:flint", "item/flint.png", 60, 60, 60, false },
	{ "minecraft:wooden_pickaxe", "item/wooden_pickaxe.png", 160, 130, 80, false },
	{ "minecraft:wooden_axe", "item/wooden_axe.png", 160, 130, 80, false },
	{ "minecraft:wooden_shovel", "item/wooden_shovel.png", 160, 130, 80, false },
	{ "minecraft:wooden_hoe", "item/wooden_hoe.png", 160, 130, 80, false },
	{ "minecraft:wooden_sword", "item/wooden_sword.png", 160, 130, 80, false },
	{ "minecraft:stone_pickaxe", "item/stone_pickaxe.png", 125, 125, 125, false },
	{ "minecraft:stone_axe", "item/stone_axe.png", 125, 125, 125, false },
	{ "minecraft:stone_shovel", "item/stone_shovel.png", 125, 125, 125, false },
	{ "minecraft:stone_hoe", "item/stone_hoe.png", 125, 125, 125, false },
	{ "minecraft:stone_sword", "item/stone_sword.png", 125, 125, 125, false },
	{ "minecraft:iron_pickaxe", "item/iron_pickaxe.png", 216, 216, 216, false },
	{ "minecraft:iron_axe", "item/iron_axe.png", 216, 216, 216, false },
	{ "minecraft:iron_shovel", "item/iron_shovel.png", 216, 216, 216, false },
	{ "minecraft:iron_hoe", "item/iron_hoe.png", 216, 216, 216, false },
	{ "minecraft:iron_sword", "item/iron_sword.png", 216, 216, 216, false },
	{ "minecraft:diamond_pickaxe", "item/diamond_pickaxe.png", 90, 230, 220, false },
	{ "minecraft:diamond_axe", "item/diamond_axe.png", 90, 230, 220, false },
	{ "minecraft:diamond_shovel", "item/diamond_shovel.png", 90, 230, 220, false },
	{ "minecraft:diamond_hoe", "item/diamond_hoe.png", 90, 230, 220, false },
	{ "minecraft:diamond_sword", "item/diamond_sword.png", 90, 230, 220, false },
};

const ItemInfo &GetItemInfo(uint8_t id)
{
	if(id >= ITEM_COUNT)
		return items[0];
	return items[id];
}

bool IsBlockItem(uint8_t id)
{
	return id >= 1 && id <= LAST_BLOCK;
}

int FindItemByName(const char *name)
{
	if(name == nullptr)
		return -1;
	for(int i = 1; i < ITEM_COUNT; i++)
		if(strcmp(items[i].name, name) == 0)
			return i;
	return -1;
}

const char *TextureBasename(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

}
