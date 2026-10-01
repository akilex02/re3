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

MC_TEST(ray_zero_direction_with_huge_maxdist_misses)
{
	World w;
	w.Set(1, 0, 0, BLOCK_STONE);
	// Zero direction with huge maxDist should miss, not hang
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 0, 0, 0, 1e30f);
	MC_CHECK(!h.hit);
}

MC_TEST(ray_nan_maxdist_misses)
{
	World w;
	w.Set(1, 0, 0, BLOCK_STONE);
	// NaN maxDist should not crash and should miss
	volatile float nan_val = 0.0f / 0.0f;
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, nan_val);
	MC_CHECK(!h.hit);
}

MC_TEST(ray_infinite_maxdist_empty_world_terminates)
{
	World w;
	// Infinite maxDist in empty world should terminate due to 4096 clamp, not hang
	RayHit h = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 1e30f);
	MC_CHECK(!h.hit);
}

MC_TEST(ray_origin_on_integer_coordinate)
{
	World w;
	w.Set(2, 0, 0, BLOCK_STONE);
	// Origin at (0,0,0), direction (1,0,0), should hit cell (2,0,0) with nx=-1
	RayHit h = RayCast(w, 0.0f, 0.0f, 0.0f, 1, 0, 0, 10);
	MC_CHECK(h.hit);
	MC_CHECK_EQ(h.x, 2);
	MC_CHECK_EQ(h.nx, -1);
}

#include "../../src/minecraft/game/McInteract.h"

MC_TEST(interact_cannot_place_when_ray_starts_inside_block)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	RayHit inside = RayCast(w, 0.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK(!McInteract::CanPlace(inside));
	RayHit miss = RayCast(w, 5.5f, 5.5f, 5.5f, 1, 0, 0, 10);
	MC_CHECK(!McInteract::CanPlace(miss));
	w.Set(4, 0, 0, BLOCK_STONE);
	RayHit face = RayCast(w, 2.5f, 0.5f, 0.5f, 1, 0, 0, 10);
	MC_CHECK(McInteract::CanPlace(face));
}

MC_TEST(body_overlaps_cell_containing_feet)
{
	MC_CHECK(McInteract::BodyOverlapsCell(0.5f, 0.5f, 6.5f, 0, 0, 5));
}

MC_TEST(body_does_not_overlap_cell_below_feet)
{
	// feet at exactly z=5.0
	MC_CHECK(!McInteract::BodyOverlapsCell(0.5f, 0.5f, 6.0f, 0, 0, 4));
	MC_CHECK(McInteract::BodyOverlapsCell(0.5f, 0.5f, 6.0f, 0, 0, 5));
}

MC_TEST(body_straddling_boundary_overlaps_both_cells)
{
	MC_CHECK(McInteract::BodyOverlapsCell(0.9f, 0.5f, 5.5f, 1, 0, 5));
	MC_CHECK(McInteract::BodyOverlapsCell(0.9f, 0.5f, 5.5f, 0, 0, 5));
}

MC_TEST(body_does_not_overlap_far_cell)
{
	MC_CHECK(!McInteract::BodyOverlapsCell(0.5f, 0.5f, 5.5f, 2, 0, 5));
}

MC_TEST(body_does_not_overlap_cell_above_head)
{
	// head at exactly z=5.0
	MC_CHECK(!McInteract::BodyOverlapsCell(0.5f, 0.5f, 4.2f, 0, 0, 5));
}

#include "McInteract.h"

MC_TEST(ray_start_offset_camera_behind_head)
{
	// camera 4 m behind the head along +x, ray points along +x
	float t0 = McInteract::RayStartOffset(-4, 0, 0, 0, 0, 0, 1, 0, 0);
	MC_CHECK_NEAR(t0, 4.0, 1e-5);
}

MC_TEST(ray_start_offset_camera_in_front_of_head)
{
	float t0 = McInteract::RayStartOffset(3, 0, 0, 0, 0, 0, 1, 0, 0);
	MC_CHECK_NEAR(t0, 0.0, 1e-6);
}

// Surface placement: cell = floor(point + normal * 0.05) per axis.
static void
CheckSurfaceCell(float px, float py, float pz, float nx, float ny, float nz, int ex, int ey, int ez)
{
	int cx = 12345, cy = 12345, cz = 12345;
	McInteract::SurfacePlacementCell(px, py, pz, nx, ny, nz, cx, cy, cz);
	MC_CHECK(cx == ex);
	MC_CHECK(cy == ey);
	MC_CHECK(cz == ez);
}

MC_TEST(surface_cell_ground)
{
	// (3.5,4.5,10.0) + (0,0,0.05) = (3.5,4.5,10.05) -> (3,4,10)
	CheckSurfaceCell(3.5f, 4.5f, 10.0f, 0, 0, 1, 3, 4, 10);
}

MC_TEST(surface_cell_wall_facing_plus_x)
{
	// (5.0,2.5,3.5) + (0.05,0,0) = (5.05,2.5,3.5) -> (5,2,3)
	CheckSurfaceCell(5.0f, 2.5f, 3.5f, 1, 0, 0, 5, 2, 3);
}

MC_TEST(surface_cell_wall_facing_minus_x)
{
	// (5.0,2.5,3.5) + (-0.05,0,0) = (4.95,2.5,3.5) -> (4,2,3)
	CheckSurfaceCell(5.0f, 2.5f, 3.5f, -1, 0, 0, 4, 2, 3);
}

MC_TEST(surface_cell_negative_coordinates)
{
	// (-0.5,-0.5,-2.0) + (0,0,0.05) = (-0.5,-0.5,-1.95) -> floor = (-1,-1,-2); an int cast would give (0,0,-1)
	CheckSurfaceCell(-0.5f, -0.5f, -2.0f, 0, 0, 1, -1, -1, -2);
}

MC_TEST(surface_cell_point_slightly_under_surface)
{
	// ground point 0.01 below the plane z=10: (3.5,4.5,9.99) + 0.05 = 10.04 -> z cell 10, still above the surface
	CheckSurfaceCell(3.5f, 4.5f, 9.99f, 0, 0, 1, 3, 4, 10);
	// wall facing -X, point 0.01 inside the wall (x=5.01): 5.01 - 0.05 = 4.96 -> x cell 4, in front of the wall
	CheckSurfaceCell(5.01f, 2.5f, 3.5f, -1, 0, 0, 4, 2, 3);
}

// Back-face normals are flipped toward the viewer: negated iff dot(normal, dir) > 0.
MC_TEST(face_normal_opposing_ray_unchanged)
{
	// ray +x, normal -x: dot = -1
	float nx = -1, ny = 0, nz = 0;
	McInteract::FaceNormalToward(nx, ny, nz, 1, 0, 0);
	MC_CHECK(nx == -1 && ny == 0 && nz == 0);
}

MC_TEST(face_normal_along_ray_negated)
{
	// ray +x, normal +x: dot = 1 -> (-1,0,0)
	float nx = 1, ny = 0, nz = 0;
	McInteract::FaceNormalToward(nx, ny, nz, 1, 0, 0);
	MC_CHECK(nx == -1 && ny == 0 && nz == 0);
}

MC_TEST(face_normal_perpendicular_unchanged)
{
	// ray +x, normal +z: dot = 0
	float nx = 0, ny = 0, nz = 1;
	McInteract::FaceNormalToward(nx, ny, nz, 1, 0, 0);
	MC_CHECK(nx == 0 && ny == 0 && nz == 1);
}
