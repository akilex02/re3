#include "common.h"

#ifdef MINECRAFT_MODE

#include "main.h"
#include "Sprite2d.h"
#include "Font.h"
#include "McBlocks.h"
#include "McHotbarLayout.h"
#include "McAtlas.h"
#include "McInteract.h"
#include "McHotbar.h"
#include "McItemTable.h"
#include "McSurvival.h"

static const float iconInset = 0.12f;	// fraction of the slot left empty on each side of the icon

// CSprite2d::DrawRect leaves ztest/zwrite on and vertex alpha off for opaque colours,
// so put the 2D pass states back before every textured draw and at the end.
static void
Set2dStates(void)
{
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
}

static void
DrawIcon(uint8 id, const CRect &rect, RwTexture *atlas)
{
	if(atlas){
		float u0, v0, u1, v1;
		Mc::BlockTileUV(id, u0, v0, u1, v1);
		// CSprite2d does not own the atlas: hand it over for the draw only, so ~CSprite2d never destroys it
		CSprite2d sprite;
		sprite.m_pTexture = atlas;
		Set2dStates();
		sprite.Draw(rect, CRGBA(255, 255, 255, 255), u0, v0, u1, v0, u0, v1, u1, v1);
		sprite.m_pTexture = nil;
		RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
	}else{
		const Mc::ItemInfo &info = Mc::GetItemInfo(id);
		CSprite2d::DrawRect(rect, CRGBA(info.r, info.g, info.b, 255));
	}
}

// "minecraft:oak_planks" -> "oak planks"
void
McHotbar::DisplayName(const char *id, char *out, size_t size)
{
	const char *p = strchr(id, ':');
	p = p ? p + 1 : id;
	size_t n = 0;
	for(; *p && n + 1 < size; p++)
		out[n++] = *p == '_' ? ' ' : *p;
	out[n] = '\0';
}

static void
SetTextStyle(float scaleX, float scaleY)
{
	CFont::SetBackgroundOff();
	CFont::SetScale(SCREEN_SCALE_X(scaleX), SCREEN_SCALE_Y(scaleY));
	CFont::SetJustifyOff();
	CFont::SetRightJustifyOff();
	CFont::SetCentreOff();
	CFont::SetPropOn();
	CFont::SetFontStyle(FONT_BANK);
	CFont::SetDropShadowPosition(1);
	CFont::SetDropColor(CRGBA(0, 0, 0, 255));
	CFont::SetColor(CRGBA(255, 255, 255, 255));
}

// Icon, wear bar and stack count of one inventory slot inside the given rectangle.
void
McHotbar::DrawSlotItem(const McSurv::Slot &slot, float x, float y, float w, float h)
{
	RwTexture *atlas = McAtlas::GetTexture();
	float inset = w * iconInset;
	DrawIcon((uint8)slot.item, CRect(x + inset, y + inset, x + w - inset, y + h - inset), atlas);
	if(slot.maxDamage > 0 && slot.damage > 0){
		// wear bar along the bottom edge: green when new, red when about to break
		float left = 1.0f - (float)slot.damage / (float)slot.maxDamage;
		if(left < 0.0f) left = 0.0f;
		float bh = SCREEN_SCALE_Y(3.0f);
		float by = y + h - bh - SCREEN_SCALE_Y(2.0f);
		float bx = x + w * 0.1f;
		float bw = w * 0.8f;
		CSprite2d::DrawRect(CRect(bx, by, bx + bw, by + bh), CRGBA(0, 0, 0, 255));
		CSprite2d::DrawRect(CRect(bx, by, bx + bw * left, by + bh), CRGBA((uint8)(255.0f * (1.0f - left)), (uint8)(255.0f * left), 0, 255));
	}
	if(slot.count > 1){
		char text[16];
		sprintf(text, "%d", slot.count);
		AsciiToUnicode(text, gUString);
		SetTextStyle(0.35f, 0.55f);
		float tw = CFont::GetStringWidth(gUString, true);
		CFont::PrintString(x + w - tw - SCREEN_SCALE_X(2.0f), y + h - SCREEN_SCALE_Y(14.0f), gUString);
		CFont::SetDropShadowPosition(0);
	}
}

// Survival hotbar: the nine inventory slots with stack counts and tool wear bars, plus the mining progress bar.
static void
DrawSurvival(void)
{
	const int count = McSurv::HOTBAR_SLOTS;
	float frame = SCREEN_SCALE_Y(2.0f);
	float barBottom = 0.0f;
	int selectedSlot = McSurv::SelectedSlot();
	McSurv::Slot selectedItem;
	selectedItem.item = 0;

	Set2dStates();
	for(int i = 0; i < count; i++){
		bool selected = i == selectedSlot;
		Mc::HotbarSlot s = Mc::HotbarSlotRect(i, count, SCREEN_WIDTH, SCREEN_HEIGHT, selected);
		if(s.w <= 0.0f)
			continue;
		if(s.y + s.h > barBottom)
			barBottom = s.y + s.h;

		CSprite2d::DrawRect(CRect(s.x, s.y, s.x + s.w, s.y + s.h), CRGBA(0, 0, 0, 150));
		McSurv::Slot slot;
		if(McSurv::GetSlot(McSurv::AREA_INV, i, slot)){
			if(selected)
				selectedItem = slot;
			McHotbar::DrawSlotItem(slot, s.x, s.y, s.w, s.h);
		}
		if(selected){
			CRGBA white(255, 255, 255, 255);
			CSprite2d::DrawRect(CRect(s.x, s.y, s.x + s.w, s.y + frame), white);
			CSprite2d::DrawRect(CRect(s.x, s.y + s.h - frame, s.x + s.w, s.y + s.h), white);
			CSprite2d::DrawRect(CRect(s.x, s.y + frame, s.x + frame, s.y + s.h - frame), white);
			CSprite2d::DrawRect(CRect(s.x + s.w - frame, s.y + frame, s.x + s.w, s.y + s.h - frame), white);
		}
	}

	// name of the selected item under the bar
	if(selectedItem.item != 0){
		char name[64];
		McHotbar::DisplayName(Mc::GetItemInfo((uint8_t)selectedItem.item).name, name, sizeof(name));
		AsciiToUnicode(name, gUString);
		SetTextStyle(0.4f, 0.6f);
		CFont::SetCentreOn();
		CFont::SetCentreSize(SCREEN_WIDTH);
		CFont::PrintString(SCREEN_WIDTH / 2.0f, barBottom + SCREEN_SCALE_Y(2.0f), gUString);
		CFont::SetDropShadowPosition(0);
	}

	// mining progress under the crosshair
	float progress = McSurv::MineProgress();
	if(progress > 0.0f){
		if(progress > 1.0f) progress = 1.0f;
		float w = SCREEN_SCALE_X(90.0f), h = SCREEN_SCALE_Y(5.0f);
		float x = SCREEN_WIDTH / 2.0f - w / 2.0f, y = SCREEN_HEIGHT * 0.58f;
		CSprite2d::DrawRect(CRect(x - 1.0f, y - 1.0f, x + w + 1.0f, y + h + 1.0f), CRGBA(0, 0, 0, 200));
		CSprite2d::DrawRect(CRect(x, y, x + w * progress, y + h), CRGBA(255, 255, 255, 230));
	}

	Set2dStates();
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
}

void
McHotbar::Draw(void)
{
	if(McSurv::IsReady()){
		DrawSurvival();
		return;
	}
	int count = Mc::HotbarCount();
	uint8 selectedBlock = McInteract::GetSelectedBlock();
	RwTexture *atlas = McAtlas::GetTexture();
	float frame = SCREEN_SCALE_Y(2.0f);
	float barBottom = 0.0f;

	Set2dStates();
	for(int i = 0; i < count; i++){
		uint8 id = Mc::HotbarBlock(i);
		bool selected = id == selectedBlock;
		Mc::HotbarSlot s = Mc::HotbarSlotRect(i, count, SCREEN_WIDTH, SCREEN_HEIGHT, selected);
		if(s.w <= 0.0f)
			continue;
		if(s.y + s.h > barBottom)
			barBottom = s.y + s.h;

		CSprite2d::DrawRect(CRect(s.x, s.y, s.x + s.w, s.y + s.h), CRGBA(0, 0, 0, 150));
		float inset = s.w * iconInset;
		DrawIcon(id, CRect(s.x + inset, s.y + inset, s.x + s.w - inset, s.y + s.h - inset), atlas);
		if(selected){
			CRGBA white(255, 255, 255, 255);
			CSprite2d::DrawRect(CRect(s.x, s.y, s.x + s.w, s.y + frame), white);
			CSprite2d::DrawRect(CRect(s.x, s.y + s.h - frame, s.x + s.w, s.y + s.h), white);
			CSprite2d::DrawRect(CRect(s.x, s.y + frame, s.x + frame, s.y + s.h - frame), white);
			CSprite2d::DrawRect(CRect(s.x + s.w - frame, s.y + frame, s.x + s.w, s.y + s.h - frame), white);
		}
	}

	// name of the selected block under the bar (the bar sits 3.5% of the screen above the bottom)
	if(Mc::HotbarIndexOfBlock(selectedBlock) >= 0){
		AsciiToUnicode(Mc::GetBlockInfo(selectedBlock).name, gUString);
		CFont::SetBackgroundOff();
		CFont::SetScale(SCREEN_SCALE_X(0.4f), SCREEN_SCALE_Y(0.6f));
		CFont::SetJustifyOff();
		CFont::SetRightJustifyOff();
		CFont::SetCentreOn();
		CFont::SetCentreSize(SCREEN_WIDTH);
		CFont::SetPropOn();
		CFont::SetFontStyle(FONT_BANK);
		CFont::SetDropShadowPosition(1);
		CFont::SetDropColor(CRGBA(0, 0, 0, 255));
		CFont::SetColor(CRGBA(255, 255, 255, 255));
		CFont::PrintString(SCREEN_WIDTH / 2.0f, barBottom + SCREEN_SCALE_Y(2.0f), gUString);
		CFont::SetDropShadowPosition(0);
	}

	Set2dStates();
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
}

#endif
