#include "common.h"

#ifdef MINECRAFT_MODE

#include "Pad.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "World.h"
#include "McMode.h"
#include "McRenderer.h"
#include "McInteract.h"
#include "McCollide.h"

static const char *saveFile = "mcworld.dat";
static const int toggleKey = 7;	// F8, zero based

static Mc::World world;
static bool active;

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
	if(world.Load(saveFile))
		printf("McMode: loaded %s (%d chunks)\n", saveFile, (int)world.ChunkCount());
}

void
Shutdown(void)
{
	if(world.ChunkCount() != 0)
		if(!world.Save(saveFile))
			printf("McMode: failed to save %s\n", saveFile);
	world.Clear();
	McRenderer::Shutdown();
	active = false;
}

void
Update(void)
{
	if(CPad::GetPad(0)->GetFJustDown(toggleKey)){
		active = !active;
		printf("McMode: Steve mode %s\n", active ? "ON" : "OFF");
	}

	if(active)
		McInteract::Update(world);

	// In a vehicle the ped is not at its own position, so only push out the ped on foot.
	if(active && FindPlayerPed() != nil && FindPlayerVehicle() == nil){
		CPlayerPed *ped = FindPlayerPed();
		CVector pos = ped->GetPosition();
		float fx = pos.x, fy = pos.y, fz = pos.z - McInteract::bodyFeetOffset;
		Mc::CollideResult r = Mc::PushOutOfBlocks(world, fx, fy, fz, McInteract::bodyHalfWidth, McInteract::bodyHeight);
		if(r.moved){
			ped->SetPosition(fx, fy, fz + McInteract::bodyFeetOffset);
			if(r.onGround){
				ped->bIsStanding = true;
				ped->m_vecMoveSpeed.z = 0.0f;
			}
		}
	}
}

void
Render(void)
{
	McRenderer::Render(world);
}

}

#endif
