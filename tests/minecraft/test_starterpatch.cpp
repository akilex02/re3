#include "mctest.h"
#include "McWorld.h"
#include "McItemTable.h"
#include "McStarterPatch.h"

using namespace Mc;

static int CountId(const World &w, int ox, int oy, int oz, uint8_t id)
{
	int n = 0;
	for(int z = oz; z < oz + 8; z++)
		for(int y = oy; y < oy + STARTER_PATCH_SIZE; y++)
			for(int x = ox; x < ox + STARTER_PATCH_SIZE; x++)
				if(w.Get(x, y, z) == id)
					n++;
	return n;
}

MC_TEST(starterpatch_has_every_survival_ingredient)
{
	World w;
	BuildStarterPatch(w, 0, 0, 10);
	MC_CHECK(CountId(w, 0, 0, 10, (uint8_t)FindItemByName("minecraft:oak_log")) >= 4);
	MC_CHECK(CountId(w, 0, 0, 10, (uint8_t)FindItemByName("minecraft:oak_leaves")) >= 8);
	MC_CHECK(CountId(w, 0, 0, 10, (uint8_t)FindItemByName("minecraft:stone")) >= 9);
	MC_CHECK(CountId(w, 0, 0, 10, (uint8_t)FindItemByName("minecraft:coal_ore")) >= 2);
	MC_CHECK(CountId(w, 0, 0, 10, (uint8_t)FindItemByName("minecraft:iron_ore")) >= 1);
	MC_CHECK(CountId(w, 0, 0, 10, (uint8_t)FindItemByName("minecraft:sand")) >= 3);
	MC_CHECK(CountId(w, 0, 0, 10, (uint8_t)FindItemByName("minecraft:gravel")) >= 3);
}

MC_TEST(starterpatch_touches_nothing_outside_its_box)
{
	World w;
	BuildStarterPatch(w, 0, 0, 10);
	for(int y = -2; y < STARTER_PATCH_SIZE + 2; y++)
		for(int x = -2; x < STARTER_PATCH_SIZE + 2; x++){
			MC_CHECK_EQ(w.Get(x, y, 9), BLOCK_AIR);	// below the patch
			MC_CHECK_EQ(w.Get(x, y, 18), BLOCK_AIR);	// above it
		}
	MC_CHECK_EQ(w.Get(-1, 0, 10), BLOCK_AIR);
	MC_CHECK_EQ(w.Get(STARTER_PATCH_SIZE, 0, 10), BLOCK_AIR);
	MC_CHECK_EQ(w.Get(0, -1, 10), BLOCK_AIR);
	MC_CHECK_EQ(w.Get(0, STARTER_PATCH_SIZE, 10), BLOCK_AIR);
}

MC_TEST(starterpatch_works_at_negative_coordinates)
{
	World w;
	BuildStarterPatch(w, -20, -30, -5);
	MC_CHECK(CountId(w, -20, -30, -5, (uint8_t)FindItemByName("minecraft:oak_log")) >= 4);
	MC_CHECK(w.ChunkCount() > 0);
}
