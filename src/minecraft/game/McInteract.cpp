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

static const float reach = 6.0f;
static uint8 selected = Mc::BLOCK_STONE;

namespace McInteract
{

// Distance from the camera to the nearest GTA geometry along the same ray, or reach+1 if none.
// The hit entity is returned in entity (nil if none).
// The player's own body (ped, and vehicle when driving) is ignored.
static float
GtaHit(const CVector &from, const CVector &to, CEntity *&entity)
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

	if(found)
		return (colPoint.point - from).Magnitude();
	entity = nil;
	return reach + 1.0f;
}

void
Update(Mc::World &world)
{
	CPad *pad = CPad::GetPad(0);
	CPlayerPed *player = FindPlayerPed();

	// no interaction without a living, controllable player outside cutscenes and replays
	if(player == nil || player->DyingOrDead() || CCutsceneMgr::IsRunning() || CCutsceneMgr::IsCutsceneProcessing() ||
	   pad->ArePlayerControlsDisabled() || CReplay::IsPlayingBack())
		return;

	bool wheelUp = pad->GetMouseWheelUpJustDown();
	bool wheelDown = pad->GetMouseWheelDownJustDown();
	if(wheelUp || wheelDown){
		int n = Mc::BLOCK_COUNT - 1;	// selectable: 1..BLOCK_COUNT-1
		int i = selected - 1;
		i += wheelUp ? 1 : -1;
		i = (i % n + n) % n;
		selected = (uint8)(i + 1);
		printf("McInteract: selected block %s\n", Mc::GetBlockInfo(selected).name);
	}

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
	float gtaDist = GtaHit(origin, origin + dir * reach, gtaEntity);

	if(breakBlock && gtaEntity != nil && gtaEntity->IsPed() && (!hit.hit || gtaDist < hit.t)){
		CPed *ped = (CPed*)gtaEntity;
		ped->InflictDamage(player, WEAPONTYPE_BASEBALLBAT, 10.0f, PEDPIECE_TORSO, 0);
		return;
	}
	if(!hit.hit)
		return;
	if(gtaDist < hit.t)
		return;	// GTA geometry is in front of the block

	if(breakBlock){
		world.Set(hit.x, hit.y, hit.z, Mc::BLOCK_AIR);
	}else if(CanPlace(hit)){
		int px = hit.x + hit.nx, py = hit.y + hit.ny, pz = hit.z + hit.nz;
		// do not place inside the player's own body
		CVector body = FindPlayerCoors();
		bool insidePlayer = BodyOverlapsCell(body.x, body.y, body.z, px, py, pz);
		if(!insidePlayer)
			world.Set(px, py, pz, selected);
	}
}

}

#endif
