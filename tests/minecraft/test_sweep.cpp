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

MC_TEST(sweep_nan_dx_returns_start_blocked)
{
	World w;
	float nan_val = __builtin_nanf("");
	SweepResult r = SweepBox(w, Car(0.0f), nan_val, 0.0f, 0.0f);
	MC_CHECK(r.blocked);
	MC_CHECK_NEAR(r.x, 0.0, 1e-6);
	MC_CHECK_NEAR(r.y, 0.0, 1e-6);
	MC_CHECK_NEAR(r.dz, 0.0, 1e-6);
}

MC_TEST(sweep_inf_dx_returns_start_blocked)
{
	World w;
	float inf_val = __builtin_inff();
	SweepResult r = SweepBox(w, Car(0.0f), inf_val, 0.0f, 0.0f);
	MC_CHECK(r.blocked);
	MC_CHECK_NEAR(r.x, 0.0, 1e-6);
	MC_CHECK_NEAR(r.y, 0.0, 1e-6);
	MC_CHECK_NEAR(r.dz, 0.0, 1e-6);
}

MC_TEST(sweep_huge_finite_dx_overflow_returns_start_blocked)
{
	World w;
	SweepResult r = SweepBox(w, Car(0.0f), 1e30f, 0.0f, 0.0f);
	MC_CHECK(r.blocked);
	MC_CHECK_NEAR(r.x, 0.0, 1e-6);
	MC_CHECK_NEAR(r.y, 0.0, 1e-6);
	MC_CHECK_NEAR(r.dz, 0.0, 1e-6);
}

MC_TEST(sweep_blocked_with_vertical_component)
{
	World w;
	// Stone floor at z in [0,1) for x in [-3,3), y in [-3,3)
	for(int x = -3; x <= 3; x++)
		for(int y = -3; y <= 3; y++)
			w.Set(x, y, 0, BLOCK_STONE);
	// Box with zBottom=4.0, zTop=5.5, centre (0,0), halfU=1, halfV=0.9, yaw=0
	OrientedBox b = { 0.0f, 0.0f, 4.0f, 5.5f, 1.0f, 0.9f, 0.0f };
	// Sweep down by 10: target zBottom = -6.0, but should stop before hitting the floor
	SweepResult r = SweepBox(w, b, 0.0f, 0.0f, -10.0f);
	MC_CHECK(r.blocked);
	MC_CHECK_NEAR(r.x, 0.0, 1e-4);
	MC_CHECK_NEAR(r.y, 0.0, 1e-4);
	// Box bottom at z=4.0 + dz must stay >= 1.0 (top of stone at z=0), so dz >= -3.0
	// Last free step is within 0.4 of that limit
	MC_CHECK(r.dz >= -3.0f - 1e-3f && r.dz <= -2.6f);
}

MC_TEST(sweep_one_metre_step_from_3_0_blocked_mid_sweep)
{
	World w;
	WallWorld(w);
	// box at x=3.0 (spans [2.0, 4.0]) moves +3: its target (6.0, span [5.0, 7.0]) is past the 1-block wall
	SweepResult r = SweepBox(w, Car(3.0f), 3.0f, 0.0f, 0.0f);
	MC_CHECK(r.blocked);
	MC_CHECK(r.x <= 4.0f + 1e-3f);
	MC_CHECK(r.x >= 3.0f);
}
