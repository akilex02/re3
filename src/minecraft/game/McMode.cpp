#include "common.h"

#ifdef MINECRAFT_MODE

#include "Pad.h"
#include "McMode.h"

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
	active = false;
}

void
Update(void)
{
	if(CPad::GetPad(0)->GetFJustDown(toggleKey)){
		active = !active;
		printf("McMode: Steve mode %s\n", active ? "ON" : "OFF");
	}
}

void
Render(void)
{
}

}

#endif
