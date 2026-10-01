#include "common.h"

#ifdef MINECRAFT_MODE

#include "Pad.h"
#include "Camera.h"
#include "Timer.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "World.h"
#include "McMode.h"
#include "McRenderer.h"
#include "McAtlas.h"
#include "McInteract.h"
#include "McCollide.h"
#include "McEntities.h"
#include "McHotbar.h"
#include "McSurvival.h"
#include "McStarterPatch.h"
#ifdef MINECRAFT_SURVIVAL
#include "mc_bridge.h"
#endif

static const char *saveFile = "mcworld.dat";
static const int toggleKey = 7;	// F8, zero based
static const int patchKey = 8;	// F9, zero based: builds the survival starter patch

static Mc::World world;
static bool active;
static uint32 savedRevision;
static uint32 lastSaveTime;
static const uint32 autosaveInterval = 45000;	// ms of game time
static const float groundTolerance = 0.1f;	// feet this far above a block top still stand on it
static bool standingOnBlocks;	// UpdateGround asserted bIsStanding on the last frame

static bool
SaveWorld(void)
{
	if(!world.Save(saveFile))
		return false;
	savedRevision = world.Revision();
	lastSaveTime = CTimer::GetTimeInMilliseconds();
	McSurv::Save();
	return true;
}

// Voxels are not GTA collision, so GTA never sees them as ground. CPed::ProcessControl decides
// "in the air" (Ped.cpp: CheckIfInTheAir/SetInTheAir) before CWorld::Process runs the collision pass,
// and that pass only re-derives bIsStanding from GTA geometry; it never clears bIsInTheAir, which only
// CPed::InTheAir -> SetLanding does when GTA ground is within 1.3 below. So, after CWorld::Process,
// assert bIsStanding while the feet rest on blocks (this is the value the next ProcessControl reads),
// land a ped that came down on blocks the way GTA lands it on ground, and give bIsStanding back to GTA
// once he leaves the blocks so he falls normally.
static void
UpdateGround(CPlayerPed *ped, float fx, float fy, float fz)
{
	// moving up (a jump launch) is never standing, so a jump from a block is not cancelled
	bool onBlocks = ped->m_vecMoveSpeed.z <= 0.0f &&
		Mc::IsStandingOnBlocks(world, fx, fy, fz, McInteract::bodyHalfWidth, groundTolerance);
	if(onBlocks){
		ped->bIsStanding = true;
		ped->m_vecMoveSpeed.z = 0.0f;
		if(ped->bIsInTheAir)
			ped->SetLanding();
	}else if(standingOnBlocks)
		ped->bIsStanding = false;	// stale: we set it, GTA ground did not
	standingOnBlocks = onBlocks;
}

namespace McMode
{

Mc::World &
GetWorld(void)
{
	return world;
}

bool
IsActive(void)
{
	return active;
}

void
Init(void)
{
	active = false;
	standingOnBlocks = false;
	if(world.Load(saveFile))
		printf("McMode: loaded %s (%d chunks)\n", saveFile, (int)world.ChunkCount());
	savedRevision = world.Revision();
	lastSaveTime = CTimer::GetTimeInMilliseconds();
	McAtlas::Init();
#ifdef MINECRAFT_SURVIVAL
	printf("McMode: survival bridge %s\n", mc_bridge_version());
#endif
}

void
Shutdown(void)
{
	// also saves an empty world, so deleting every block persists
	if(world.Revision() != savedRevision)
		if(!SaveWorld())
			printf("McMode: failed to save %s\n", saveFile);
	McSurv::Shutdown();
	world.Clear();
	McAtlas::Shutdown();
	McRenderer::Shutdown();
	active = false;
}

void
Update(void)
{
	McAtlas::Update();
	McSurv::Update();

	if(CPad::GetPad(0)->GetFJustDown(toggleKey)){
		active = !active;
		printf("McMode: Steve mode %s\n", active ? "ON" : "OFF");
	}

	if(active)
		McInteract::Update(world);

	if(active && CPad::GetPad(0)->GetFJustDown(patchKey) && McInteract::CanInteract()){
		CPlayerPed *player = FindPlayerPed();
		CVector p = player->GetPosition();
		// three blocks ahead on the diagonal so it never overlaps the player's own body
		Mc::BuildStarterPatch(world, (int)floorf(p.x) + 3, (int)floorf(p.y) + 3, (int)floorf(p.z - McInteract::bodyFeetOffset));
		printf("McMode: starter patch built\n");
	}

	if(world.Revision() != savedRevision && CTimer::GetTimeInMilliseconds() - lastSaveTime >= autosaveInterval){
		if(SaveWorld())
			printf("McMode: autosaved %s (%d chunks)\n", saveFile, (int)world.ChunkCount());
		else
			lastSaveTime = CTimer::GetTimeInMilliseconds();	// do not retry every frame
	}

	// In a vehicle the ped is not at its own position, so only push out the ped on foot.
	CPlayerPed *ped = FindPlayerPed();
	if(active && ped != nil && FindPlayerVehicle() == nil){
		CVector pos = ped->GetPosition();
		float fx = pos.x, fy = pos.y, fz = pos.z - McInteract::bodyFeetOffset;
		Mc::CollideResult r = Mc::PushOutOfBlocks(world, fx, fy, fz, McInteract::bodyHalfWidth, McInteract::bodyHeight);
		if(r.moved)
			ped->SetPosition(fx, fy, fz + McInteract::bodyFeetOffset);
		UpdateGround(ped, fx, fy, fz);
	}else if(standingOnBlocks){
		if(ped != nil)
			ped->bIsStanding = false;
		standingOnBlocks = false;
	}

	McEntities::Update(world);
}

void
Render(void)
{
	McRenderer::Render(world);
}

// Called from Render2dStuff() after the HUD.
void
Render2d(void)
{
	if(!active || !McInteract::CanInteract() || TheCamera.m_WideScreenOn)
		return;
	McHotbar::Draw();
}

}

#endif
