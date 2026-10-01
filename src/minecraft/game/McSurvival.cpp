#include "common.h"

#ifdef MINECRAFT_MODE
#ifdef MINECRAFT_SURVIVAL

#include "Timer.h"
#include "mc_bridge.h"
#include "McItemTable.h"
#include "McSurvival.h"

static const char jarFile[] = "mcassets/client.jar";
static const char catalogFile[] = "mcassets/item-catalog-26.3.json";
static const char saveFile[] = "mcsurvival.dat";
static const uint32 retryInterval = 3000;	// ms of game time between attempts to create the core
static const uint32 tickMs = 50;	// the survival core runs at 20 Hz
static const uint32 maxCatchUpMs = 250;	// a long frame never fires a burst of mining ticks

static McSurvival *core;
static int selected;
static uint32 accumMs;
static float progress;
static uint32 lastAttempt;
static bool attempted;
static bool reportedMissing;

static bool
FileExists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if(f == nil)
		return false;
	fclose(f);
	return true;
}

static int32_t
GetCb(void *ctx, int32_t x, int32_t y, int32_t z)
{
	return ((Mc::World*)ctx)->Get(x, y, z);
}

static void
SetCb(void *ctx, int32_t x, int32_t y, int32_t z, int32_t id)
{
	((Mc::World*)ctx)->Set(x, y, z, (uint8_t)id);
}

static void
TryCreate(void)
{
	if(!FileExists(jarFile) || !FileExists(catalogFile)){
		if(!reportedMissing){
			printf("McSurvival: waiting for %s and %s (run scripts/minecraft/fetch-catalogs.sh from the game folder)\n", jarFile, catalogFile);
			reportedMissing = true;
		}
		return;
	}
	const char *names[Mc::ITEM_COUNT];
	for(int i = 0; i < Mc::ITEM_COUNT; i++)
		names[i] = Mc::GetItemInfo((uint8_t)i).name;
	core = mc_survival_create(jarFile, catalogFile, names, Mc::ITEM_COUNT);
	if(core == nil){
		printf("McSurvival: survival core not available: %s\n", mc_survival_last_error());
		return;
	}
	if(mc_survival_load(core, saveFile))
		printf("McSurvival: loaded %s\n", saveFile);
	printf("McSurvival: survival core ready\n");
}

namespace McSurv
{

void
Update(void)
{
	if(core != nil)
		return;
	uint32 now = CTimer::GetTimeInMilliseconds();
	if(attempted && now - lastAttempt < retryInterval)
		return;
	attempted = true;
	lastAttempt = now;
	TryCreate();
}

bool
Save(void)
{
	if(core == nil)
		return false;
	bool ok = mc_survival_save(core, saveFile) != 0;
	if(!ok)
		printf("McSurvival: failed to save %s\n", saveFile);
	return ok;
}

void
Shutdown(void)
{
	if(core != nil){
		mc_inv_close(core);
		Save();
		mc_survival_destroy(core);
	}
	core = nil;
	selected = 0;
	accumMs = 0;
	progress = 0.0f;
	attempted = false;
	reportedMissing = false;
}

bool
IsReady(void)
{
	return core != nil;
}

McSurvival *
Core(void)
{
	return core;
}

bool
GetSlot(int area, int index, Slot &out)
{
	McStack s;
	out.item = 0;
	out.count = 0;
	out.damage = 0;
	out.maxDamage = 0;
	if(core == nil || !mc_inv_get(core, area, index, &s))
		return false;
	out.item = s.item;
	out.count = s.count;
	out.damage = s.damage;
	out.maxDamage = s.max_damage;
	return s.item != 0;
}

int
SelectedSlot(void)
{
	return selected;
}

void
SetSelectedSlot(int slot)
{
	if(slot >= 0 && slot < HOTBAR_SLOTS)
		selected = slot;
}

bool
Consume(int slot)
{
	return core != nil && mc_inv_consume(core, slot, 1) != 0;
}

void
StopMining(void)
{
	accumMs = 0;
	progress = 0.0f;
	if(core != nil)
		mc_survival_stop_mining(core);
}

float
MineProgress(void)
{
	return progress;
}

void
Mine(Mc::World &world, const double eye[3], const double dir[3], bool attacking, bool onGround)
{
	if(core == nil)
		return;
	if(!attacking){
		StopMining();
		return;
	}
	accumMs += CTimer::GetTimeStepInMilliseconds();
	if(accumMs > maxCatchUpMs)
		accumMs = maxCatchUpMs;
	while(accumMs >= tickMs){
		accumMs -= tickMs;
		McMineResult r;
		if(!mc_survival_mine(core, GetCb, SetCb, &world, eye, dir, 1, onGround ? 1 : 0, selected, &r))
			return;
		progress = r.progress;
		if(r.broken){
			printf("McSurvival: broke %s (+%d items, %d lost%s)\n", Mc::GetItemInfo((uint8_t)r.block_id).name,
				r.drops_added, r.drops_lost, r.tool_broke ? ", tool broke" : "");
			if(r.drops_lost > 0)
				printf("McSurvival: inventory full or unknown drops: %d item(s) lost\n", r.drops_lost);
			progress = 0.0f;
		}
	}
}

}

#endif
#endif
