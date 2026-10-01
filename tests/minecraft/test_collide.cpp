#include "mctest.h"
#include "McCollide.h"

using namespace Mc;

MC_TEST(collide_no_overlap_changes_nothing)
{
	World w;
	w.Set(5, 5, 5, BLOCK_STONE);
	float x = 0.5f, y = 0.5f, z = 0.0f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(!r.moved);
	MC_CHECK(!r.onGround);
	MC_CHECK_NEAR(z, 0.0, 1e-6);
}

MC_TEST(collide_sunk_into_floor_is_pushed_up_and_grounded)
{
	World w;
	for(int x = -2; x <= 2; x++)
		for(int y = -2; y <= 2; y++)
			w.Set(x, y, 0, BLOCK_STONE);     // floor occupies z in [0,1)
	float x = 0.5f, y = 0.5f, z = 0.9f;      // feet 0.1 below the floor surface
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(r.onGround);
	MC_CHECK_NEAR(z, 1.0, 1e-3);
	MC_CHECK_NEAR(x, 0.5, 1e-6);
	MC_CHECK_NEAR(y, 0.5, 1e-6);
}

MC_TEST(collide_standing_exactly_on_floor_is_not_moved)
{
	World w;
	for(int x = -2; x <= 2; x++)
		for(int y = -2; y <= 2; y++)
			w.Set(x, y, 0, BLOCK_STONE);
	float x = 0.5f, y = 0.5f, z = 1.0f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(!r.moved);
}

MC_TEST(collide_wall_pushes_sideways_not_up)
{
	World w;
	for(int z = 0; z < 4; z++)
		w.Set(1, 0, z, BLOCK_STONE);         // wall at x in [1,2)
	float x = 0.8f, y = 0.5f, z = 0.0f;      // box x range [0.5,1.1] overlaps the wall by 0.1
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(!r.onGround);
	MC_CHECK_NEAR(x, 0.7, 1e-3);             // pushed back to x + halfWidth == 1.0
	MC_CHECK_NEAR(z, 0.0, 1e-6);
}

MC_TEST(collide_ceiling_pushes_down)
{
	World w;
	for(int x = -2; x <= 2; x++)
		for(int y = -2; y <= 2; y++)
			w.Set(x, y, 2, BLOCK_STONE);     // ceiling occupies z in [2,3)
	float x = 0.5f, y = 0.5f, z = 0.3f;      // head at 2.1, 0.1 into the ceiling
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(!r.onGround);
	MC_CHECK_NEAR(z, 0.2, 1e-3);
}

MC_TEST(collide_works_in_negative_coordinates)
{
	World w;
	for(int x = -10; x <= -6; x++)
		for(int y = -10; y <= -6; y++)
			w.Set(x, y, -5, BLOCK_DIRT);     // floor top at z = -4
	float x = -8.5f, y = -8.5f, z = -4.1f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(r.onGround);
	MC_CHECK_NEAR(z, -4.0, 1e-3);
}

MC_TEST(collide_glass_is_solid)
{
	World w;
	w.Set(0, 0, 0, BLOCK_GLASS);
	float x = 0.5f, y = 0.5f, z = 0.9f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
}

MC_TEST(collide_ground_flag_survives_wall_overlap)
{
	// Floor of stone at z=0 (top at z=1) spanning x,y in [-2,2]; wall of stone at x=1 for z in 1..3
	// (occupies x in [1,2), z in [1,4)). Box half width 0.3, height 1.8, centred at x=0.8, y=0.5,
	// feet z=0.9: it sinks 0.1 into the floor and its x range [0.5,1.1] overlaps the wall by 0.1.
	// Both penetrations are 0.1, so the order of resolution is arbitrary, but the final state must be:
	// feet on the floor (z=1.0), out of the wall (x<=0.7), and onGround must stay true.
	World w;
	for(int x = -2; x <= 2; x++)
		for(int y = -2; y <= 2; y++)
			w.Set(x, y, 0, BLOCK_STONE);
	for(int y = -2; y <= 2; y++)
		for(int z = 1; z <= 3; z++)
			w.Set(1, y, z, BLOCK_STONE);
	float x = 0.8f, y = 0.5f, z = 0.9f;
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.moved);
	MC_CHECK(r.onGround);
	MC_CHECK_NEAR(z, 1.0, 1e-3);
	MC_CHECK(x <= 0.7f + 1e-3f);
}

MC_TEST(ground_feet_exactly_on_block_top)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);	// top at z = 1
	MC_CHECK(IsStandingOnBlocks(w, 0.5f, 0.5f, 1.0f, 0.3f, 0.1f));
}

MC_TEST(ground_feet_a_hair_inside_block_top)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	MC_CHECK(IsStandingOnBlocks(w, 0.5f, 0.5f, 0.99999f, 0.3f, 0.1f));
}

MC_TEST(ground_feet_within_tolerance_above_top)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	MC_CHECK(IsStandingOnBlocks(w, 0.5f, 0.5f, 1.05f, 0.3f, 0.1f));
	MC_CHECK(!IsStandingOnBlocks(w, 0.5f, 0.5f, 1.2f, 0.3f, 0.1f));
}

MC_TEST(ground_corner_overhanging_edge_still_stands)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	// footprint x in [0.9,1.5]: only the -x corners are over the block
	MC_CHECK(IsStandingOnBlocks(w, 1.2f, 0.5f, 1.0f, 0.3f, 0.1f));
	// footprint x in [1.01,1.61]: walked off the edge
	MC_CHECK(!IsStandingOnBlocks(w, 1.31f, 0.5f, 1.0f, 0.3f, 0.1f));
}

MC_TEST(ground_touching_side_face_only_is_not_ground)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	// footprint x in [1.0,1.6] touches the block's +x face exactly
	MC_CHECK(!IsStandingOnBlocks(w, 1.3f, 0.5f, 1.0f, 0.3f, 0.1f));
}

MC_TEST(ground_wall_at_feet_level_is_not_ground)
{
	World w;
	w.Set(0, 0, 1, BLOCK_STONE);	// occupies z in [1,2): beside the body, not under it
	MC_CHECK(!IsStandingOnBlocks(w, 0.5f, 0.5f, 1.0f, 0.3f, 0.1f));
}

MC_TEST(ground_empty_world_is_not_ground)
{
	World w;
	MC_CHECK(!IsStandingOnBlocks(w, 0.5f, 0.5f, 1.0f, 0.3f, 0.1f));
}

MC_TEST(ground_works_in_negative_coordinates)
{
	World w;
	w.Set(-9, -9, -5, BLOCK_DIRT);	// top at z = -4
	MC_CHECK(IsStandingOnBlocks(w, -8.5f, -8.5f, -4.0f, 0.3f, 0.1f));
	MC_CHECK(IsStandingOnBlocks(w, -8.5f, -8.5f, -4.00001f, 0.3f, 0.1f));
	MC_CHECK(!IsStandingOnBlocks(w, -8.5f, -8.5f, -3.8f, 0.3f, 0.1f));
}

MC_TEST(ground_after_push_out_is_standing)
{
	World w;
	w.Set(3, 3, 7, BLOCK_STONE);
	float x = 3.5f, y = 3.5f, z = 7.93f;	// sunk into the top after a gravity step
	CollideResult r = PushOutOfBlocks(w, x, y, z, 0.3f, 1.8f);
	MC_CHECK(r.onGround);
	MC_CHECK(IsStandingOnBlocks(w, x, y, z, 0.3f, 0.1f));
}
