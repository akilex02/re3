#include "common.h"

#ifdef MINECRAFT_MODE

#include "Pad.h"
#include "Camera.h"
#include "Ped.h"
#include "Weapon.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "Vehicle.h"
#include "World.h"
#include "Collision.h"
#include "CutsceneMgr.h"
#include "Replay.h"
#include "McInteract.h"
#include "McHotbarLayout.h"

static const float reach = 6.0f;
static uint8 selected = Mc::BLOCK_STONE;

namespace McInteract
{

// Distance from the camera to the nearest GTA geometry along the same ray, or reach+1 if none.
// The hit entity is returned in entity (nil if none), the hit position and surface normal in point and normal.
// The player's own body (ped, and vehicle when driving) is ignored.
static float
GtaHit(const CVector &from, const CVector &to, CEntity *&entity, CVector &point, CVector &normal)
{
	CColPoint colPoint;
	entity = nil;
	CPlayerPed *ped = FindPlayerPed();
	CVehicle *veh = FindPlayerVehicle();
	CEntity *prevIgnore = CWorld::pIgnoreEntity;
	bool savedPedCollision = false;

	if(veh){
		CWorld::pIgnoreEntity = veh;
		if(ped){
			savedPedCollision = ped->bUsesCollision;
			ped->bUsesCollision = false;
		}
	}else
		CWorld::pIgnoreEntity = ped;

	bool found = CWorld::ProcessLineOfSight(from, to, colPoint, entity, true, true, true, true, false, true);

	CWorld::pIgnoreEntity = prevIgnore;
	if(veh && ped)
		ped->bUsesCollision = savedPedCollision;

	if(found){
		point = colPoint.point;
		normal = colPoint.normal;
		return (colPoint.point - from).Magnitude();
	}
	entity = nil;
	return reach + 1.0f;
}

bool
CanInteract(void)
{
	CPlayerPed *player = FindPlayerPed();
	return !(player == nil || player->DyingOrDead() || CCutsceneMgr::IsRunning() || CCutsceneMgr::IsCutsceneProcessing() ||
		CPad::GetPad(0)->ArePlayerControlsDisabled() || CReplay::IsPlayingBack());
}

uint8
GetSelectedBlock(void)
{
	return selected;
}

void
SetSelectedBlock(uint8 id)
{
	if(id < 1 || id >= Mc::BLOCK_COUNT)
		return;
	selected = id;
	printf("McInteract: selected block %s\n", Mc::GetBlockInfo(selected).name);
}

void
Update(Mc::World &world)
{
	CPad *pad = CPad::GetPad(0);

	// no interaction without a living, controllable player outside cutscenes and replays
	if(!CanInteract())
		return;
	CPlayerPed *player = FindPlayerPed();

	bool wheelUp = pad->GetMouseWheelUpJustDown();
	bool wheelDown = pad->GetMouseWheelDownJustDown();
	if(wheelUp || wheelDown){
		int n = Mc::BLOCK_COUNT - 1;	// selectable: 1..BLOCK_COUNT-1
		int i = selected - 1;
		i += wheelUp ? 1 : -1;
		i = (i % n + n) % n;
		SetSelectedBlock((uint8)(i + 1));
	}

	// number keys 1.. pick a hotbar slot (just pressed, not held)
	int keys = Mc::HotbarCount() < 9 ? Mc::HotbarCount() : 9;
	for(int k = 0; k < keys; k++)
		if(pad->GetCharJustDown('1' + k))
			SetSelectedBlock(Mc::HotbarBlock(k));

	bool breakBlock = pad->GetLeftMouseJustDown();
	bool placeBlock = pad->GetRightMouseJustDown();
	if(!breakBlock && !placeBlock)
		return;

	// The third-person camera can be outside the player's own walls, so the ray starts at the point
	// of the camera ray nearest the player's head (head is about 0.7 above the ped origin).
	CVector camPos = TheCamera.GetPosition();
	CVector dir = TheCamera.GetForward();
	dir.Normalise();
	CVector head = player->GetPosition() + CVector(0.0f, 0.0f, 0.7f);
	float t0 = RayStartOffset(camPos.x, camPos.y, camPos.z, head.x, head.y, head.z, dir.x, dir.y, dir.z);
	CVector origin = camPos + dir * t0;
	Mc::RayHit hit = Mc::RayCast(world, origin.x, origin.y, origin.z, dir.x, dir.y, dir.z, reach);
	CEntity *gtaEntity;
	CVector gtaPoint, gtaNormal;
	float gtaDist = GtaHit(origin, origin + dir * reach, gtaEntity, gtaPoint, gtaNormal);
	bool gtaNearest = gtaEntity != nil && (!hit.hit || gtaDist < hit.t);

	if(breakBlock){
		if(gtaNearest){
			// breaking never affects GTA geometry, only peds can be hit
			if(gtaEntity->IsPed()){
				CPed *ped = (CPed*)gtaEntity;
				printf("McInteract: hit ped (model %d health %.1f)\n", ped->GetModelIndex(), (double)ped->m_fHealth);
				bool damaged = ped->InflictDamage(player, WEAPONTYPE_BASEBALLBAT, 10.0f, PEDPIECE_TORSO, 0);
				printf("McInteract: damage applied: %d\n", (int)damaged);
			}else
				printf("McInteract: left click hit non-ped GTA entity (type %d)\n", (int)gtaEntity->GetType());
			return;
		}
		if(!hit.hit)
			return;
		world.Set(hit.x, hit.y, hit.z, Mc::BLOCK_AIR);
		printf("McInteract: broke block at (%d,%d,%d)\n", hit.x, hit.y, hit.z);
		return;
	}

	// place: against the nearest of the voxel hit and the GTA hit
	int px, py, pz;
	const char *kind;
	if(gtaNearest){
		if(gtaEntity->IsPed()){
			printf("McInteract: nothing placed, target is a ped\n");
			return;
		}
		FaceNormalToward(gtaNormal.x, gtaNormal.y, gtaNormal.z, dir.x, dir.y, dir.z);
		SurfacePlacementCell(gtaPoint.x, gtaPoint.y, gtaPoint.z, gtaNormal.x, gtaNormal.y, gtaNormal.z, px, py, pz);
		if(world.Get(px, py, pz) != Mc::BLOCK_AIR){
			printf("McInteract: nothing placed, target cell occupied\n");
			return;
		}
		kind = "surface";
	}else if(CanPlace(hit)){
		px = hit.x + hit.nx;
		py = hit.y + hit.ny;
		pz = hit.z + hit.nz;
		kind = "voxel";
	}else{
		printf("McInteract: nothing placed, no target\n");
		return;
	}
	// do not place inside the player's own body
	CVector body = FindPlayerCoors();
	if(BodyOverlapsCell(body.x, body.y, body.z, px, py, pz)){
		printf("McInteract: nothing placed, blocked by player\n");
		return;
	}
	world.Set(px, py, pz, selected);
	printf("McInteract: placed %s at (%d,%d,%d) [%s]\n", Mc::GetBlockInfo(selected).name, px, py, pz, kind);
}

}

#endif
