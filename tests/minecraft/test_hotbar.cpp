#include "mctest.h"
#include "McHotbarLayout.h"

using namespace Mc;

MC_TEST(hotbar_slot_positions_at_1080p)
{
	// size = 64.8, gap = 9.72, total = 4*64.8 + 3*9.72 = 288.36, left = (1920-288.36)/2 = 815.82
	HotbarSlot s0 = HotbarSlotRect(0, 4, 1920.0f, 1080.0f, false);
	HotbarSlot s1 = HotbarSlotRect(1, 4, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(s0.x, 815.82, 0.01);
	MC_CHECK_NEAR(s1.x, 815.82 + 64.8 + 9.72, 0.01);
	MC_CHECK_NEAR(s0.y, 1080.0 - 37.8 - 64.8, 0.01);	// 977.4
	MC_CHECK_NEAR(s0.w, 64.8, 0.01);
	MC_CHECK_NEAR(s0.h, 64.8, 0.01);
}

MC_TEST(hotbar_is_centred_and_symmetric)
{
	float screenW = 2560.0f, screenH = 1440.0f;
	HotbarSlot first = HotbarSlotRect(0, 4, screenW, screenH, false);
	HotbarSlot last = HotbarSlotRect(3, 4, screenW, screenH, false);
	float leftMargin = first.x;
	float rightMargin = screenW - (last.x + last.w);
	MC_CHECK_NEAR(leftMargin, rightMargin, 0.01);
}

MC_TEST(hotbar_scales_with_screen_height)
{
	HotbarSlot a = HotbarSlotRect(0, 4, 1920.0f, 1080.0f, false);
	HotbarSlot b = HotbarSlotRect(0, 4, 2560.0f, 1440.0f, false);
	MC_CHECK_NEAR(b.w / a.w, 1440.0 / 1080.0, 1e-4);
	// bottom margin is 3.5% of the height at both sizes
	MC_CHECK_NEAR((1080.0 - (a.y + a.h)) / 1080.0, 0.035, 1e-4);
	MC_CHECK_NEAR((1440.0 - (b.y + b.h)) / 1440.0, 0.035, 1e-4);
}

MC_TEST(hotbar_selected_slot_grows_around_the_same_centre)
{
	HotbarSlot n = HotbarSlotRect(2, 4, 1920.0f, 1080.0f, false);
	HotbarSlot s = HotbarSlotRect(2, 4, 1920.0f, 1080.0f, true);
	MC_CHECK_NEAR(s.w, n.w * 1.1, 0.01);
	MC_CHECK_NEAR(s.h, n.h * 1.1, 0.01);
	MC_CHECK_NEAR(s.x + s.w / 2, n.x + n.w / 2, 0.01);
	MC_CHECK_NEAR(s.y + s.h / 2, n.y + n.h / 2, 0.01);
}

MC_TEST(hotbar_single_slot_is_centred_and_ultrawide_is_centred)
{
	HotbarSlot one = HotbarSlotRect(0, 1, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(one.x + one.w / 2, 960.0, 0.01);
	HotbarSlot uw = HotbarSlotRect(1, 3, 3440.0f, 1440.0f, false);   // middle of 3
	MC_CHECK_NEAR(uw.x + uw.w / 2, 1720.0, 0.01);
}

MC_TEST(hotbar_invalid_arguments_give_empty_rect)
{
	HotbarSlot z = { 1, 1, 1, 1 };
	z = HotbarSlotRect(0, 0, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
	z = HotbarSlotRect(-1, 4, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
	z = HotbarSlotRect(4, 4, 1920.0f, 1080.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
	z = HotbarSlotRect(0, 4, 0.0f, 1080.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
	z = HotbarSlotRect(0, 4, 1920.0f, -5.0f, false);
	MC_CHECK_NEAR(z.w, 0.0, 1e-9);
}

MC_TEST(hotbar_tile_uv_matches_the_atlas_layout)
{
	float u0, v0, u1, v1;
	const float inset = 0.5f / 64.0f;
	BlockTileUV(BLOCK_STONE, u0, v0, u1, v1);    // id 2 -> tile (2,0)
	MC_CHECK_NEAR(u0, 0.5 + inset, 1e-6);
	MC_CHECK_NEAR(u1, 0.75 - inset, 1e-6);
	MC_CHECK_NEAR(v0, 0.0 + inset, 1e-6);
	MC_CHECK_NEAR(v1, 0.25 - inset, 1e-6);
	BlockTileUV(5, u0, v0, u1, v1);              // id 5 -> tile (1,1)
	MC_CHECK_NEAR(u0, 0.25 + inset, 1e-6);
	MC_CHECK_NEAR(v0, 0.25 + inset, 1e-6);
	MC_CHECK_NEAR(v1, 0.5 - inset, 1e-6);
}

MC_TEST(hotbar_block_index_mapping)
{
	MC_CHECK_EQ(HotbarCount(), BLOCK_COUNT - 1);
	MC_CHECK_EQ(HotbarBlock(0), BLOCK_DIRT);
	MC_CHECK_EQ(HotbarBlock(HotbarCount() - 1), BLOCK_COUNT - 1);
	MC_CHECK_EQ(HotbarBlock(-1), BLOCK_AIR);
	MC_CHECK_EQ(HotbarBlock(HotbarCount()), BLOCK_AIR);
	MC_CHECK_EQ(HotbarIndexOfBlock(BLOCK_DIRT), 0);
	MC_CHECK_EQ(HotbarIndexOfBlock(BLOCK_GLASS), 3);
	MC_CHECK_EQ(HotbarIndexOfBlock(BLOCK_AIR), -1);
	MC_CHECK_EQ(HotbarIndexOfBlock(200), -1);
	for(int i = 0; i < HotbarCount(); i++)
		MC_CHECK_EQ(HotbarIndexOfBlock(HotbarBlock(i)), i);
}
