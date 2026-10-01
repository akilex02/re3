#include "common.h"

#ifdef MINECRAFT_MODE

#include "Ped.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "World.h"
#include "Pools.h"
#include "McEntities.h"
#include "McInteract.h"
#include "McCollide.h"

namespace McEntities
{

static bool
NearBounds(const CVector &p, int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
	return p.x >= minX - 2 && p.x <= maxX + 3 &&
		p.y >= minY - 2 && p.y <= maxY + 3 &&
		p.z >= minZ - 3 && p.z <= maxZ + 4;
}

// Remove only the velocity component pointing into the block (against the push direction); no bounce.
static void
CancelVelocityInto(CVector &vel, float dx, float dy, float dz)
{
	float len = sqrtf(dx * dx + dy * dy + dz * dz);
	if(len < 1e-6f)
		return;
	CVector n(dx / len, dy / len, dz / len);
	float d = DotProduct(vel, n);
	if(d < 0.0f)
		vel -= n * d;
}

static void
UpdatePeds(Mc::World &world, int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
	CPedPool *pool = CPools::GetPedPool();
	CPed *player = FindPlayerPed();
	for(int32 i = 0; i < pool->GetSize(); i++){
		CPed *ped = pool->GetSlot(i);
		if(ped == nil || ped == player || ped->bInVehicle || !ped->bUsesCollision || ped->DyingOrDead())
			continue;
		CVector pos = ped->GetPosition();
		if(!NearBounds(pos, minX, minY, minZ, maxX, maxY, maxZ))
			continue;
		float fx = pos.x, fy = pos.y, fz = pos.z - McInteract::bodyFeetOffset;
		Mc::CollideResult r = Mc::PushOutOfBlocks(world, fx, fy, fz, McInteract::bodyHalfWidth, McInteract::bodyHeight);
		if(!r.moved)
			continue;
		float dx = fx - pos.x, dy = fy - pos.y, dz = (fz + McInteract::bodyFeetOffset) - pos.z;
		ped->SetPosition(fx, fy, fz + McInteract::bodyFeetOffset);
		CancelVelocityInto(ped->m_vecMoveSpeed, dx, dy, dz);
		if(r.onGround){
			ped->bIsStanding = true;
			ped->m_vecMoveSpeed.z = 0.0f;
			if(ped->bIsInTheAir)
				ped->SetLanding();
		}
	}
}

void
Update(Mc::World &world)
{
	int minX, minY, minZ, maxX, maxY, maxZ;
	if(!world.GetBounds(minX, minY, minZ, maxX, maxY, maxZ))
		return;
	UpdatePeds(world, minX, minY, minZ, maxX, maxY, maxZ);
}

}

#endif
