#include "mctest.h"
#include "McRay.h"

using namespace Mc;

MC_TEST(ray_hits_block_along_plus_x)
{
	World w;
	w.Set(3, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, 3); MC_CHECK_EQ(h.y, 0); MC_CHECK_EQ(h.z, 0);
	MC_CHECK_EQ(h.nx, -1); MC_CHECK_EQ(h.ny, 0); MC_CHECK_EQ(h.nz, 0);
	MC_CHECK_NEAR(h.t, 2.5, 1e-4);
}

MC_TEST(ray_hits_block_along_minus_x_negative_cells)
{
	World w;
	w.Set(-2, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, -1, 0, 0, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, -2);
	MC_CHECK_EQ(h.nx, 1);
	MC_CHECK_NEAR(h.t, 1.5, 1e-4);
}

MC_TEST(ray_hits_top_face_going_down)
{
	World w;
	w.Set(0, 0, 0, BLOCK_DIRT);
	RayHit h = RayCast(w, 0.5f, 0.5f, 5.5f, 0, 0, -1, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.nz, 1);
	MC_CHECK_NEAR(h.t, 4.5, 1e-4);
}

MC_TEST(ray_hits_block_in_negative_chunk)
{
	World w;
	w.Set(-20, -20, -20, BLOCK_WOOD);
	RayHit h = RayCast(w, -10.5f, -19.5f, -19.5f, -1, 0, 0, 20);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, -20);
	MC_CHECK_EQ(h.nx, 1);
}

MC_TEST(ray_misses_beyond_max_distance)
{
	World w;
	w.Set(10, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 5);
	MC_CHECK(!h.hit);
}

MC_TEST(ray_misses_in_empty_world)
{
	World w;
	MC_CHECK(!RayCast(w, 0, 0, 0, 0.3f, 0.4f, 0.5f, 100).hit);
}

MC_TEST(ray_zero_direction_misses)
{
	World w;
	w.Set(1, 0, 0, BLOCK_STONE);
	MC_CHECK(!RayCast(w, 0.5f, 0.5f, 0.5f, 0, 0, 0, 10).hit);
}

MC_TEST(ray_starting_inside_block_hits_with_zero_normal)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.nx, 0); MC_CHECK_EQ(h.ny, 0); MC_CHECK_EQ(h.nz, 0);
	MC_CHECK_NEAR(h.t, 0, 1e-6);
}

MC_TEST(ray_returns_nearest_block)
{
	World w;
	w.Set(2, 0, 0, BLOCK_STONE);
	w.Set(5, 0, 0, BLOCK_STONE);
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK_EQ(h.x, 2);
}

MC_TEST(ray_diagonal_hits_expected_cell)
{
	World w;
	w.Set(3, 3, 0, BLOCK_STONE);
	// from the centre of (0,0) toward the centre of (3,3): passes through the cell corners
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 1, 0, 20);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, 3); MC_CHECK_EQ(h.y, 3);
}
