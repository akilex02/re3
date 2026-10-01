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
		const Mc::BlockInfo &info = Mc::GetBlockInfo(id);
		CSprite2d::DrawRect(rect, CRGBA(info.r, info.g, info.b, 255));
	}
}

void
McHotbar::Draw(void)
{
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
