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
