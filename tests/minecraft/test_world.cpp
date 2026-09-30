#include "mctest.h"
#include "McWorld.h"

using namespace Mc;

MC_TEST(world_floor_div_mod_negative)
{
	MC_CHECK_EQ(FloorDiv(0, 16), 0);
	MC_CHECK_EQ(FloorDiv(15, 16), 0);
	MC_CHECK_EQ(FloorDiv(16, 16), 1);
	MC_CHECK_EQ(FloorDiv(-1, 16), -1);
	MC_CHECK_EQ(FloorDiv(-16, 16), -1);
	MC_CHECK_EQ(FloorDiv(-17, 16), -2);
	MC_CHECK_EQ(FloorMod(-1, 16), 15);
	MC_CHECK_EQ(FloorMod(-16, 16), 0);
	MC_CHECK_EQ(FloorMod(17, 16), 1);
}

MC_TEST(world_empty_is_air)
{
	World w;
	MC_CHECK_EQ(w.Get(0, 0, 0), BLOCK_AIR);
	MC_CHECK_EQ(w.Get(-500, 200, -3), BLOCK_AIR);
	MC_CHECK_EQ(w.ChunkCount(), 0);
}

MC_TEST(world_set_get_roundtrip_including_negative)
{
	World w;
	MC_CHECK(w.Set(3, 4, 5, BLOCK_STONE));
	MC_CHECK(w.Set(-1, -1, -1, BLOCK_DIRT));
	MC_CHECK(w.Set(-17, 40, -33, BLOCK_WOOD));
	MC_CHECK_EQ(w.Get(3, 4, 5), BLOCK_STONE);
	MC_CHECK_EQ(w.Get(-1, -1, -1), BLOCK_DIRT);
	MC_CHECK_EQ(w.Get(-17, 40, -33), BLOCK_WOOD);
	MC_CHECK_EQ(w.Get(2, 4, 5), BLOCK_AIR);
	MC_CHECK_EQ(w.ChunkCount(), 3);
}

MC_TEST(world_set_same_value_reports_no_change)
{
	World w;
	MC_CHECK(w.Set(1, 1, 1, BLOCK_DIRT));
	MC_CHECK(!w.Set(1, 1, 1, BLOCK_DIRT));
}

MC_TEST(world_set_air_in_missing_chunk_allocates_nothing)
{
	World w;
	MC_CHECK(!w.Set(100, 100, 100, BLOCK_AIR));
	MC_CHECK_EQ(w.ChunkCount(), 0);
}

MC_TEST(world_empty_chunk_is_freed)
{
	World w;
	w.Set(1, 1, 1, BLOCK_DIRT);
	w.Set(2, 1, 1, BLOCK_DIRT);
	MC_CHECK_EQ(w.ChunkCount(), 1);
	w.Set(1, 1, 1, BLOCK_AIR);
	MC_CHECK_EQ(w.ChunkCount(), 1);
	w.Set(2, 1, 1, BLOCK_AIR);
	MC_CHECK_EQ(w.ChunkCount(), 0);
}

MC_TEST(world_set_marks_chunk_and_border_neighbours_dirty)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);      // chunk (0,0,0)
	w.Set(-1, 0, 0, BLOCK_DIRT);     // chunk (-1,0,0), neighbour across the x=0 border
	ChunkPos a = { 0, 0, 0 };
	ChunkPos b = { -1, 0, 0 };
	w.FindChunk(a)->dirty = false;
	w.FindChunk(b)->dirty = false;
	w.Set(0, 5, 5, BLOCK_STONE);     // on the x=0 border of chunk a
	MC_CHECK(w.FindChunk(a)->dirty);
	MC_CHECK(w.FindChunk(b)->dirty);
}

MC_TEST(world_set_interior_does_not_dirty_neighbours)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);
	w.Set(-1, 0, 0, BLOCK_DIRT);
	ChunkPos b = { -1, 0, 0 };
	w.FindChunk(b)->dirty = false;
	w.Set(8, 8, 8, BLOCK_STONE);
	MC_CHECK(!w.FindChunk(b)->dirty);
}

MC_TEST(world_unknown_block_id_is_air_info)
{
	MC_CHECK_EQ(GetBlockInfo(200).transparent, true);
	MC_CHECK(GetBlockInfo(BLOCK_GLASS).transparent);
	MC_CHECK(!GetBlockInfo(BLOCK_STONE).transparent);
}
