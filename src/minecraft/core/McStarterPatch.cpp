#include "McStarterPatch.h"
#include "McItemTable.h"

namespace Mc {

static uint8_t Id(const char *name)
{
	int id = FindItemByName(name);
	return id > 0 ? (uint8_t)id : (uint8_t)BLOCK_AIR;
}

void BuildStarterPatch(World &world, int ox, int oy, int oz)
{
	const uint8_t stone = Id("minecraft:stone");
	const uint8_t coal = Id("minecraft:coal_ore");
	const uint8_t iron = Id("minecraft:iron_ore");
	const uint8_t log = Id("minecraft:oak_log");
	const uint8_t leaves = Id("minecraft:oak_leaves");
	const uint8_t sand = Id("minecraft:sand");
	const uint8_t gravel = Id("minecraft:gravel");

	// stone block, 3x3 and two layers high, with ores in two corners and one edge
	for(int z = 0; z < 2; z++)
		for(int y = 0; y < 3; y++)
			for(int x = 0; x < 3; x++)
				world.Set(ox + x, oy + y, oz + z, stone);
	world.Set(ox + 0, oy + 0, oz + 0, coal);
	world.Set(ox + 2, oy + 2, oz + 0, coal);
	world.Set(ox + 2, oy + 0, oz + 1, iron);

	// sand and gravel rows
	for(int x = 0; x < 3; x++){
		world.Set(ox + x, oy + 4, oz, sand);
		world.Set(ox + x, oy + 5, oz, gravel);
	}

	// tree: four logs and a crown of leaves around the top
	for(int z = 0; z < 4; z++)
		world.Set(ox + 5, oy + 5, oz + z, log);
	for(int y = 4; y <= 6; y++)
		for(int x = 4; x <= 6; x++){
			if(!(x == 5 && y == 5))
				world.Set(ox + x, oy + y, oz + 3, leaves);
			world.Set(ox + x, oy + y, oz + 4, leaves);
		}
}

}
