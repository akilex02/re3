#include "common.h"

#ifdef MINECRAFT_MODE
#ifdef MINECRAFT_SURVIVAL

#include "main.h"
#include "Pad.h"
#include "Sprite2d.h"
#include "Font.h"
#include "PlayerPed.h"
#include "CutsceneMgr.h"
#include "Replay.h"
#include "Camera.h"
#include "World.h"
#include "mc_bridge.h"
#include "McItemTable.h"
#include "McInventoryLayout.h"
#include "McInteract.h"
#include "McSurvival.h"
#include "McHotbar.h"
#include "McInventoryUI.h"

static const int maxSlots = 64;

// The player ped reads the pad before McMode::Update runs, and CCamera::Process clears PLAYERCONTROL_CAMERA every frame
// after it, so that bit is never set when the ped reads its controls. PLAYERCONTROL_UNK10 is used by nothing else.
static const uint8 PLAYERCONTROL_MCINVENTORY = PLAYERCONTROL_UNK10;

static bool open;
static bool workbench;
static float mouseX, mouseY;
static Mc::UiSlot slots[maxSlots];
static int numSlots;

// CSprite2d::DrawRect leaves ztest/zwrite on; put the 2D pass states back before textured draws and at the end.
static void
Set2dStates(void)
{
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
}

static void
Relayout(void)
{
	numSlots = Mc::BuildInventoryLayout(workbench, SCREEN_WIDTH, SCREEN_HEIGHT, slots, maxSlots);
}

// The mouse deltas move our own cursor and are then zeroed so the camera does not turn (same trick as the debug menu).
static void
UpdateMouse(CPad *pad)
{
	int dirX = MousePointerStateHelper.bInvertHorizontally ? -1 : 1;
	int dirY = MousePointerStateHelper.bInvertVertically ? -1 : 1;
	mouseX += pad->NewMouseControllerState.x * dirX;
	mouseY += pad->NewMouseControllerState.y * dirY;
	if(mouseX < 0.0f) mouseX = 0.0f;
	if(mouseY < 0.0f) mouseY = 0.0f;
	if(mouseX > SCREEN_WIDTH) mouseX = SCREEN_WIDTH;
	if(mouseY > SCREEN_HEIGHT) mouseY = SCREEN_HEIGHT;
	pad->NewMouseControllerState.x = 0.0f;
	pad->NewMouseControllerState.y = 0.0f;
}

static void
Click(const Mc::UiSlot &s, bool right, bool shift)
{
	McSurvival *core = McSurv::Core();
	if(core == nil)
		return;
	if(s.area == Mc::AREA_OUT2 || s.area == Mc::AREA_OUT3)
		mc_inv_take_output(core, s.area == Mc::AREA_OUT3 ? 1 : 0, shift ? 1 : 0);
	else
		mc_inv_click(core, s.area, s.index, right ? 1 : 0, shift ? 1 : 0);
}

namespace McInventoryUI
{

bool
IsOpen(void)
{
	return open;
}

void
Open(bool wb)
{
	if(open || !McSurv::IsReady() || FindPlayerVehicle() != nil)
		return;
	CPad *pad = CPad::GetPad(0);
	open = true;
	workbench = wb;
	mouseX = SCREEN_WIDTH / 2.0f;
	mouseY = SCREEN_HEIGHT / 2.0f;
	// stop the player (movement, attacks) while the screen is up
	pad->SetDisablePlayerControls(PLAYERCONTROL_MCINVENTORY);
	McSurv::StopMining();
	Relayout();
}

void
Close(void)
{
	if(!open)
		return;
	open = false;
	McSurvival *core = McSurv::Core();
	if(core != nil){
		int lost = mc_inv_close(core);
		if(lost > 0)
			printf("McInventoryUI: %d stack(s) did not fit in the inventory and were lost\n", lost);
	}
	CPad::GetPad(0)->SetEnablePlayerControls(PLAYERCONTROL_MCINVENTORY);
}

void
Update(void)
{
	CPad *pad = CPad::GetPad(0);
	CPlayerPed *player = FindPlayerPed();

	if(!McSurv::IsReady()){
		Close();
		return;
	}
	if(!open){
		if(pad->GetCharJustDown('E') && McInteract::CanInteract())
			Open(false);
		return;
	}

	// the screen goes away when the player cannot use it any more
	if(player == nil || player->DyingOrDead() || CCutsceneMgr::IsRunning() || CCutsceneMgr::IsCutsceneProcessing() || CReplay::IsPlayingBack() ||
			TheCamera.m_WideScreenOn || FindPlayerVehicle() != nil ||
			(pad->DisablePlayerControls & ~PLAYERCONTROL_MCINVENTORY) != 0){	// scripts, garages, phone: the screen is hidden or unusable
		Close();
		return;
	}
	if(pad->GetCharJustDown('E')){
		Close();
		return;
	}
	pad->SetDisablePlayerControls(PLAYERCONTROL_MCINVENTORY);	// re-assert every frame

	Relayout();
	UpdateMouse(pad);
	bool left = pad->GetLeftMouseJustDown();
	bool right = pad->GetRightMouseJustDown();
	if(left || right){
		Mc::UiSlot hit;
		if(Mc::HitTestSlot(slots, numSlots, mouseX, mouseY, hit))
			Click(hit, right, pad->GetShift());
	}
}

void
Render2d(void)
{
	if(!open || numSlots == 0)
		return;

	Set2dStates();
	CSprite2d::DrawRect(CRect(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT), CRGBA(0, 0, 0, 130));

	// panel behind the slots
	float minX = slots[0].x, minY = slots[0].y, maxX = slots[0].x + slots[0].w, maxY = slots[0].y + slots[0].h;
	for(int i = 1; i < numSlots; i++){
		if(slots[i].x < minX) minX = slots[i].x;
		if(slots[i].y < minY) minY = slots[i].y;
		if(slots[i].x + slots[i].w > maxX) maxX = slots[i].x + slots[i].w;
		if(slots[i].y + slots[i].h > maxY) maxY = slots[i].y + slots[i].h;
	}
	float pad = slots[0].w * 0.4f;
	CSprite2d::DrawRect(CRect(minX - pad, minY - pad, maxX + pad, maxY + pad), CRGBA(40, 40, 40, 235));

	Mc::UiSlot hover;
	bool hovering = Mc::HitTestSlot(slots, numSlots, mouseX, mouseY, hover);
	McSurv::Slot hoverItem;
	hoverItem.item = 0;

	for(int i = 0; i < numSlots; i++){
		const Mc::UiSlot &s = slots[i];
		bool isHover = hovering && hover.area == s.area && hover.index == s.index;
		CSprite2d::DrawRect(CRect(s.x, s.y, s.x + s.w, s.y + s.h), isHover ? CRGBA(110, 110, 110, 220) : CRGBA(0, 0, 0, 150));
		McSurv::Slot item;
		if(McSurv::GetSlot(s.area, s.index, item)){
			if(isHover)
				hoverItem = item;
			McHotbar::DrawSlotItem(item, s.x, s.y, s.w, s.h);
		}
		// bar showing the crafting direction, between the grid and its output
		if(s.area == Mc::AREA_OUT2 || s.area == Mc::AREA_OUT3){
			float cy = s.y + s.h * 0.5f, len = s.w * 0.7f;
			CSprite2d::DrawRect(CRect(s.x - len - s.w * 0.1f, cy - SCREEN_SCALE_Y(2.0f), s.x - s.w * 0.1f, cy + SCREEN_SCALE_Y(2.0f)), CRGBA(200, 200, 200, 220));
		}
	}

	// the stack held by the cursor
	McSurv::Slot held;
	float size = slots[0].w;
	if(McSurv::GetSlot(McSurv::AREA_CURSOR, 0, held))
		McHotbar::DrawSlotItem(held, mouseX - size * 0.5f, mouseY - size * 0.5f, size, size);

	if(held.item == 0){
		float c = SCREEN_SCALE_X(3.0f);
		CSprite2d::DrawRect(CRect(mouseX - c, mouseY - c, mouseX + c, mouseY + c), CRGBA(255, 255, 255, 230));
	}

	// tooltip for the item under the mouse (not while a stack is held)
	if(hoverItem.item != 0 && held.item == 0){
		char name[64];
		McHotbar::DisplayName(Mc::GetItemInfo((uint8_t)hoverItem.item).name, name, sizeof(name));
		AsciiToUnicode(name, gUString);
		CFont::SetBackgroundOff();
		CFont::SetScale(SCREEN_SCALE_X(0.4f), SCREEN_SCALE_Y(0.6f));
		CFont::SetJustifyOff();
		CFont::SetRightJustifyOff();
		CFont::SetCentreOff();
		CFont::SetPropOn();
		CFont::SetFontStyle(FONT_BANK);
		CFont::SetDropShadowPosition(1);
		CFont::SetDropColor(CRGBA(0, 0, 0, 255));
		CFont::SetColor(CRGBA(255, 255, 255, 255));
		CFont::PrintString(mouseX + SCREEN_SCALE_X(16.0f), mouseY + SCREEN_SCALE_Y(8.0f), gUString);
		CFont::SetDropShadowPosition(0);
	}

	Set2dStates();
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
}

}

#endif
#endif
