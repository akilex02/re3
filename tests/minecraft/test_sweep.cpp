#include "mctest.h"
#include "McOrientedBox.h"

using namespace Mc;

static World *WallWorld(World &w)
{
	for(int y = -6; y <= 6; y++)
		for(int z = 0; z <= 1; z++)
			w.Set(5, y, z, BLOCK_STONE);	// wall at x in [5,6)
	return &w;
}

static OrientedBox Car(float x)
{
	OrientedBox b = { x, 0.0f, 0.0f, 1.5f, 1.0f, 0.9f, 0.0f };	// x extent +-1 (u along +X)
	return b;
}

MC_TEST(sweep_fast_mover_is_stopped_by_wall)
{
	World w;
	WallWorld(w);
	SweepResult r = SweepBox(w, Car(0.0f), 100.0f, 0.0f, 0.0f);	// 100 m in one frame
	MC_CHECK(r.blocked);
	MC_CHECK(r.x >= 3.5f && r.x <= 4.0f + 1e-3f);		// box max x = x+1 must stay <= 5
	MC_CHECK_NEAR(r.dz, 0.0, 1e-6);
}

MC_TEST(sweep_one_metre_step_does_not_tunnel_through_one_block_wall)
{
	World w;
	WallWorld(w);
	// box at x=3.9 (max 4.9) moves +3: its target (6.9) is past the 1-block wall
	SweepResult r = SweepBox(w, Car(3.9f), 3.0f, 0.0f, 0.0f);
	MC_CHECK(r.blocked);
	MC_CHECK(r.x <= 4.0f + 1e-3f);
}

MC_TEST(sweep_no_wall_reaches_target)
{
	World w;
	SweepResult r = SweepBox(w, Car(0.0f), 12.0f, 3.0f, 0.5f);
	MC_CHECK(!r.blocked);
	MC_CHECK_NEAR(r.x, 12.0, 1e-4);
	MC_CHECK_NEAR(r.y, 3.0, 1e-4);
	MC_CHECK_NEAR(r.dz, 0.5, 1e-4);
}

MC_TEST(sweep_parallel_to_wall_is_not_blocked)
{
	World w;
	WallWorld(w);
	OrientedBox b = Car(0.0f);			// x in [-1,1], far from the wall
	SweepResult r = SweepBox(w, b, 0.0f, 5.0f, 0.0f);
	MC_CHECK(!r.blocked);
}

MC_TEST(sweep_starting_inside_blocks_returns_target)
{
	World w;
	WallWorld(w);
	OrientedBox b = Car(5.5f);			// already inside the wall
	SweepResult r = SweepBox(w, b, 2.0f, 0.0f, 0.0f);
	MC_CHECK(!r.blocked);
	MC_CHECK_NEAR(r.x, 7.5, 1e-4);
}

MC_TEST(sweep_negative_direction_and_coordinates)
{
	World w;
	for(int y = -6; y <= 6; y++)
		for(int z = 0; z <= 1; z++)
			w.Set(-6, y, z, BLOCK_STONE);	// wall at x in [-6,-5)
	SweepResult r = SweepBox(w, Car(0.0f), -100.0f, 0.0f, 0.0f);
	MC_CHECK(r.blocked);
	MC_CHECK(r.x <= -4.0f + 1e-3f + 0.4f && r.x >= -4.0f - 1e-3f);	// box min x = x-1 must stay >= -5
}

MC_TEST(sweep_zero_move_is_not_blocked)
{
	World w;
	WallWorld(w);
	SweepResult r = SweepBox(w, Car(0.0f), 0.0f, 0.0f, 0.0f);
	MC_CHECK(!r.blocked);
	MC_CHECK_NEAR(r.x, 0.0, 1e-6);
}
