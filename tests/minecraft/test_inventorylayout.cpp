#include "mctest.h"
#include <math.h>
#include "McInventoryLayout.h"

using namespace Mc;

static const int MAXS = 64;

static int Build(bool workbench, float w, float h, UiSlot *s)
{
	return BuildInventoryLayout(workbench, w, h, s, MAXS);
}

MC_TEST(layout_counts)
{
	UiSlot s[MAXS];
	MC_CHECK_EQ(Build(false, 1920.0f, 1080.0f, s), 45);	// 36 inventory + 4 armor + 2x2 grid + output
	MC_CHECK_EQ(Build(true, 1920.0f, 1080.0f, s), 46);	// 36 inventory + 3x3 grid + output
}

MC_TEST(layout_rejects_bad_arguments)
{
	UiSlot s[MAXS];
	MC_CHECK_EQ(BuildInventoryLayout(false, 0.0f, 1080.0f, s, MAXS), 0);
	MC_CHECK_EQ(BuildInventoryLayout(false, 1920.0f, -5.0f, s, MAXS), 0);
	MC_CHECK_EQ(BuildInventoryLayout(false, NAN, 1080.0f, s, MAXS), 0);
	MC_CHECK_EQ(BuildInventoryLayout(false, 1920.0f, 1080.0f, s, 10), 0);	// output array too small
	MC_CHECK_EQ(BuildInventoryLayout(false, 1920.0f, 1080.0f, nullptr, MAXS), 0);
}

MC_TEST(layout_every_slot_appears_exactly_once)
{
	UiSlot s[MAXS];
	int n = Build(false, 1920.0f, 1080.0f, s);
	int inv[40] = { 0 };
	int grid[4] = { 0 };
	int out = 0;
	for(int i = 0; i < n; i++){
		if(s[i].area == AREA_INV){
			MC_CHECK(s[i].index >= 0 && s[i].index < 40);
			inv[s[i].index]++;
		}else if(s[i].area == AREA_GRID2){
			MC_CHECK(s[i].index >= 0 && s[i].index < 4);
			grid[s[i].index]++;
		}else if(s[i].area == AREA_OUT2){
			out++;
		}else
			MC_CHECK(false);
	}
	for(int i = 0; i < 40; i++)
		MC_CHECK_EQ(inv[i], 1);
	for(int i = 0; i < 4; i++)
		MC_CHECK_EQ(grid[i], 1);
	MC_CHECK_EQ(out, 1);
}

MC_TEST(layout_workbench_has_a_3x3_grid_and_no_armor)
{
	UiSlot s[MAXS];
	int n = Build(true, 1920.0f, 1080.0f, s);
	int grid = 0, out = 0, armor = 0;
	for(int i = 0; i < n; i++){
		if(s[i].area == AREA_GRID3) grid++;
		if(s[i].area == AREA_OUT3) out++;
		if(s[i].area == AREA_INV && s[i].index >= 36) armor++;
	}
	MC_CHECK_EQ(grid, 9);
	MC_CHECK_EQ(out, 1);
	MC_CHECK_EQ(armor, 0);
}

MC_TEST(layout_slots_do_not_overlap_and_stay_on_screen)
{
	float sizes[2][2] = { { 1920.0f, 1080.0f }, { 2560.0f, 1440.0f } };
	for(int k = 0; k < 2; k++)
		for(int wb = 0; wb < 2; wb++){
			UiSlot s[MAXS];
			float w = sizes[k][0], h = sizes[k][1];
			int n = Build(wb != 0, w, h, s);
			for(int i = 0; i < n; i++){
				MC_CHECK(s[i].w > 0.0f && s[i].h > 0.0f);
				MC_CHECK(s[i].x >= 0.0f && s[i].y >= 0.0f);
				MC_CHECK(s[i].x + s[i].w <= w && s[i].y + s[i].h <= h);
				for(int j = i + 1; j < n; j++){
					bool apart = s[i].x + s[i].w <= s[j].x + 1e-3f || s[j].x + s[j].w <= s[i].x + 1e-3f ||
						s[i].y + s[i].h <= s[j].y + 1e-3f || s[j].y + s[j].h <= s[i].y + 1e-3f;
					MC_CHECK(apart);
				}
			}
		}
}

MC_TEST(layout_panel_is_centred)
{
	UiSlot s[MAXS];
	int n = Build(false, 1920.0f, 1080.0f, s);
	float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
	for(int i = 0; i < n; i++){
		if(s[i].x < minX) minX = s[i].x;
		if(s[i].x + s[i].w > maxX) maxX = s[i].x + s[i].w;
		if(s[i].y < minY) minY = s[i].y;
		if(s[i].y + s[i].h > maxY) maxY = s[i].y + s[i].h;
	}
	MC_CHECK_NEAR(minX, 1920.0f - maxX, 0.5);
	MC_CHECK_NEAR(minY, 1080.0f - maxY, 0.5);
}

MC_TEST(layout_hotbar_row_is_below_the_main_grid)
{
	UiSlot s[MAXS];
	int n = Build(false, 1920.0f, 1080.0f, s);
	float hotbarY = -1.0f, mainY = -1.0f;
	for(int i = 0; i < n; i++)
		if(s[i].area == AREA_INV){
			if(s[i].index == 0) hotbarY = s[i].y;
			if(s[i].index == 9) mainY = s[i].y;
		}
	MC_CHECK(hotbarY > mainY);
	MC_CHECK(mainY > 0.0f);
}

MC_TEST(hit_test_finds_a_slot_centre_and_misses_gaps)
{
	UiSlot s[MAXS];
	int n = Build(false, 1920.0f, 1080.0f, s);
	UiSlot hit;
	for(int i = 0; i < n; i++){
		MC_CHECK(HitTestSlot(s, n, s[i].x + s[i].w * 0.5f, s[i].y + s[i].h * 0.5f, hit));
		MC_CHECK_EQ(hit.area, s[i].area);
		MC_CHECK_EQ(hit.index, s[i].index);
	}
	// the gap between two neighbouring slots of the hotbar row
	UiSlot a, b;
	for(int i = 0; i < n; i++){
		if(s[i].area == AREA_INV && s[i].index == 0) a = s[i];
		if(s[i].area == AREA_INV && s[i].index == 1) b = s[i];
	}
	float gapX = (a.x + a.w + b.x) * 0.5f;
	MC_CHECK(!HitTestSlot(s, n, gapX, a.y + a.h * 0.5f, hit));
	MC_CHECK(!HitTestSlot(s, n, -10.0f, 5.0f, hit));
	MC_CHECK(!HitTestSlot(s, n, 5000.0f, 5.0f, hit));
	MC_CHECK(!HitTestSlot(s, n, NAN, 5.0f, hit));
	MC_CHECK(!HitTestSlot(s, 0, 100.0f, 100.0f, hit));
	MC_CHECK(!HitTestSlot(nullptr, n, 100.0f, 100.0f, hit));
}
