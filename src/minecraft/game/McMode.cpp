#include "common.h"

#ifdef MINECRAFT_MODE

#include "Pad.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "World.h"
#include "McMode.h"
#include "McRenderer.h"

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
		// TEMPORARY (Task 6): removed in Task 7
		if(active){
			CVector p = FindPlayerCoors() + FindPlayerPed()->GetForward() * 3.0f;
			world.Set((int)floorf(p.x), (int)floorf(p.y), (int)floorf(FindPlayerCoors().z), Mc::BLOCK_STONE);
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
