#include "mctest.h"
#include "McWorld.h"

using namespace Mc;

MC_TEST(bounds_empty_world_has_none)
{
	World w;
	int a, b, c, d, e, f;
	MC_CHECK(!w.GetBounds(a, b, c, d, e, f));
}

MC_TEST(bounds_single_chunk)
{
	World w;
	w.Set(3, 4, 5, BLOCK_STONE);
	int x0, y0, z0, x1, y1, z1;
	MC_CHECK(w.GetBounds(x0, y0, z0, x1, y1, z1));
	MC_CHECK_EQ(x0, 0); MC_CHECK_EQ(y0, 0); MC_CHECK_EQ(z0, 0);
	MC_CHECK_EQ(x1, 15); MC_CHECK_EQ(y1, 15); MC_CHECK_EQ(z1, 15);
}

MC_TEST(bounds_union_with_negative_chunks)
{
	World w;
	w.Set(-1, -1, -1, BLOCK_DIRT);   // chunk (-1,-1,-1): cells -16..-1
	w.Set(40, 0, 0, BLOCK_DIRT);     // chunk (2,0,0): cells 32..47
	int x0, y0, z0, x1, y1, z1;
	MC_CHECK(w.GetBounds(x0, y0, z0, x1, y1, z1));
	MC_CHECK_EQ(x0, -16); MC_CHECK_EQ(y0, -16); MC_CHECK_EQ(z0, -16);
	MC_CHECK_EQ(x1, 47); MC_CHECK_EQ(y1, 15); MC_CHECK_EQ(z1, 15);
}

MC_TEST(bounds_shrinks_when_chunk_is_freed)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);
	w.Set(100, 0, 0, BLOCK_DIRT);
	w.Set(100, 0, 0, BLOCK_AIR);
	int x0, y0, z0, x1, y1, z1;
	MC_CHECK(w.GetBounds(x0, y0, z0, x1, y1, z1));
	MC_CHECK_EQ(x1, 15);
}
