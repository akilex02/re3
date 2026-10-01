#include "mctest.h"
#include "McOrientedBox.h"
#include <math.h>

using namespace Mc;

static OrientedBox MakeBox(float x, float y, float zb, float zt, float hu, float hv, float yaw)
{
	OrientedBox b = { x, y, zb, zt, hu, hv, yaw };
	return b;
}

static void Wall(World &w, int x, int y0, int y1, int z0, int z1)
{
	for(int y = y0; y <= y1; y++)
		for(int z = z0; z <= z1; z++)
			w.Set(x, y, z, BLOCK_STONE);
}

MC_TEST(obox_empty_world_does_not_overlap)
{
	World w;
	OrientedBox b = MakeBox(0, 0, 0, 1, 2, 1, 0.3f);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(!p.moved);
}

MC_TEST(obox_yaw0_wall_pushes_back_along_x)
{
	World w;
	Wall(w, 5, -3, 3, 0, 0);
	// box x range 1.5..5.5 overlaps wall cells (x from 5) by 0.5; y range +-0.9 within the wall
	OrientedBox b = MakeBox(3.5f, 0.0f, 0.0f, 1.0f, 2.0f, 0.9f, 0.0f);
	MC_CHECK(BoxOverlapsBlocks(w, b));
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(!p.onTop);
	MC_CHECK_NEAR(b.x, 3.0, 1e-3);
	MC_CHECK_NEAR(b.y, 0.0, 1e-3);
	MC_CHECK_NEAR(p.dx, -0.5, 1e-3);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
}

MC_TEST(obox_yaw90_uses_rotated_extents)
{
	World w;
	Wall(w, 5, -3, 3, 0, 0);
	// yaw 90 deg: u points along +Y (half 2.0), v along -X (half 0.9); x range 3.6..5.4 overlaps by 0.4
	OrientedBox b = MakeBox(4.5f, 0.0f, 0.0f, 1.0f, 2.0f, 0.9f, 1.57079633f);
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK_NEAR(b.x, 4.1, 1e-3);
	MC_CHECK_NEAR(b.y, 0.0, 1e-3);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
}

MC_TEST(obox_yaw45_is_resolved_without_overlap)
{
	World w;
	Wall(w, 5, -6, 6, 0, 0);
	OrientedBox b = MakeBox(3.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f, 0.78539816f);
	MC_CHECK(BoxOverlapsBlocks(w, b));
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
	MC_CHECK(b.x < 3.0f);                 // moved away from the wall
	float d = sqrtf(p.dx * p.dx + p.dy * p.dy + p.dz * p.dz);
	MC_CHECK(d < 1.0f);                   // a small correction, not a teleport
}

MC_TEST(obox_floor_pushes_up_and_reports_on_top)
{
	World w;
	for(int x = -5; x <= 5; x++)
		for(int y = -5; y <= 5; y++)
			w.Set(x, y, 0, BLOCK_STONE);   // floor top at z = 1
	OrientedBox b = MakeBox(0.5f, 0.5f, 0.9f, 2.7f, 1.0f, 1.0f, 0.3f);
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(p.onTop);
	MC_CHECK_NEAR(b.zBottom, 1.0, 1e-3);
	MC_CHECK_NEAR(b.zTop, 2.8, 1e-3);
	MC_CHECK_NEAR(p.dz, 0.1, 1e-3);
}

MC_TEST(obox_ceiling_pushes_down)
{
	World w;
	for(int x = -5; x <= 5; x++)
		for(int y = -5; y <= 5; y++)
			w.Set(x, y, 3, BLOCK_STONE);   // ceiling bottom at z = 3
	OrientedBox b = MakeBox(0.5f, 0.5f, 1.3f, 3.1f, 1.0f, 1.0f, 0.0f);
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(!p.onTop);
	MC_CHECK_NEAR(b.zTop, 3.0, 1e-3);
}

MC_TEST(obox_touching_faces_do_not_overlap)
{
	World w;
	for(int x = -5; x <= 5; x++)
		for(int y = -5; y <= 5; y++)
			w.Set(x, y, 0, BLOCK_STONE);
	OrientedBox b = MakeBox(0.5f, 0.5f, 1.0f, 2.8f, 1.0f, 1.0f, 0.7f);
	MC_CHECK(!BoxOverlapsBlocks(w, b));
	MC_CHECK(!PushBoxOutOfBlocks(w, b).moved);
}

MC_TEST(obox_negative_coordinates)
{
	World w;
	for(int x = -10; x <= -6; x++)
		for(int y = -10; y <= -6; y++)
			w.Set(x, y, -5, BLOCK_DIRT);    // floor top at z = -4
	OrientedBox b = MakeBox(-7.5f, -8.5f, -4.1f, -2.3f, 1.0f, 0.8f, -0.4f);
	BoxPush p = PushBoxOutOfBlocks(w, b);
	MC_CHECK(p.moved);
	MC_CHECK(p.onTop);
	MC_CHECK_NEAR(b.zBottom, -4.0, 1e-3);
}

MC_TEST(obox_inside_solid_terminates)
{
	World w;
	for(int x = -3; x <= 3; x++)
		for(int y = -3; y <= 3; y++)
			for(int z = -3; z <= 3; z++)
				w.Set(x, y, z, BLOCK_STONE);
	OrientedBox b = MakeBox(0.0f, 0.0f, -0.5f, 0.5f, 1.0f, 1.0f, 0.2f);
	BoxPush p = PushBoxOutOfBlocks(w, b);          // must return (8 iteration cap), whatever it does
	MC_CHECK(p.moved);
	MC_CHECK(sqrtf(p.dx * p.dx + p.dy * p.dy + p.dz * p.dz) < 40.0f);
}
