#include "common.h"

#ifdef MINECRAFT_MODE

#include <cmath>
#include <unordered_map>

#include "Ped.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "World.h"
#include "Vehicle.h"
#include "ColModel.h"
#include "Pools.h"
#include "McEntities.h"
#include "McInteract.h"
#include "McCollide.h"
#include "McOrientedBox.h"

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

// Vehicle origin at the end of the previous frame (after resolution). Rebuilt every frame.
static std::unordered_map<const CVehicle*, CVector> ms_prevPos;

static bool
IsFiniteVec(const CVector &v)
{
	return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Oriented box around the colmodel bounding box. Local +Y is forward, +X is right; pitch and roll are ignored.
static Mc::OrientedBox
BuildVehicleBox(CVehicle *veh)
{
	CColModel *col = veh->GetColModel();
	const CVector &mn = col->boundingBox.min, &mx = col->boundingBox.max;
	CVector c = veh->GetMatrix() * ((mn + mx) * 0.5f);
	CVector fwd = veh->GetMatrix().GetForward();
	float halfZ = (mx.z - mn.z) * 0.5f;
	Mc::OrientedBox b;
	b.x = c.x;
	b.y = c.y;
	b.zBottom = c.z - halfZ;
	b.zTop = c.z + halfZ;
	b.halfU = (mx.y - mn.y) * 0.5f;
	b.halfV = (mx.x - mn.x) * 0.5f;
	b.yaw = atan2f(fwd.y, fwd.x);
	return b;
}

// n points into the block. Removes the velocity component along n, with a little restitution (0.2).
static void
VehicleVelocityResponse(CVector &vel, const CVector &n)
{
	float d = DotProduct(vel, n);
	if(d > 0.0f)
		vel -= n * (1.2f * d);
}

// Accepted limitation: wheels use their own suspension probes which ignore voxels, so a car pushed on top
// of a block is held at the top but gets no wheel ground contact.
static void
UpdateVehicles(Mc::World &world, int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
	std::unordered_map<const CVehicle*, CVector> newPrev;
	CVehiclePool *pool = CPools::GetVehiclePool();
	for(int32 i = 0; i < pool->GetSize(); i++){
		CVehicle *veh = pool->GetSlot(i);
		if(veh == nil || !veh->bUsesCollision || !veh->IsCar())
			continue;
		CVector pos = veh->GetPosition();
		if(!IsFiniteVec(pos) || !NearBounds(pos, minX, minY, minZ, maxX, maxY, maxZ))
			continue;
		Mc::OrientedBox box = BuildVehicleBox(veh);
		if(!std::isfinite(box.x) || !std::isfinite(box.y) || !std::isfinite(box.zBottom) ||
		   !std::isfinite(box.halfU) || !std::isfinite(box.halfV) || !std::isfinite(box.yaw)){
			newPrev[veh] = pos;
			continue;
		}
		Mc::OrientedBox cur = box;
		CVector corr(0.0f, 0.0f, 0.0f), travel(0.0f, 0.0f, 0.0f);
		bool swept = false;

		std::unordered_map<const CVehicle*, CVector>::const_iterator it = ms_prevPos.find(veh);
		if(it != ms_prevPos.end()){
			CVector disp = pos - it->second;
			float len = disp.Magnitude();
			if(IsFiniteVec(disp) && len > 0.3f){
				Mc::OrientedBox from = box;
				from.x -= disp.x;
				from.y -= disp.y;
				from.zBottom -= disp.z;
				from.zTop -= disp.z;
				Mc::SweepResult sr = Mc::SweepBox(world, from, disp.x, disp.y, disp.z);
				if(sr.blocked){
					corr.x = sr.x - box.x;
					corr.y = sr.y - box.y;
					corr.z = (from.zBottom + sr.dz) - box.zBottom;
					travel = disp * (1.0f / len);
					swept = true;
					cur.x += corr.x;
					cur.y += corr.y;
					cur.zBottom += corr.z;
					cur.zTop += corr.z;
				}
			}
		}

		Mc::BoxPush push = Mc::PushBoxOutOfBlocks(world, cur);
		CVector total = corr;
		if(push.moved){
			total.x += push.dx;
			total.y += push.dy;
			total.z += push.dz;
		}
		float tlen = total.Magnitude();
		// Sane maximum: a car spawned inside blocks must not be teleported.
		if(!IsFiniteVec(total) || tlen > 3.0f){
			newPrev[veh] = pos;
			continue;
		}
		if(swept || push.moved){
			if(swept)
				VehicleVelocityResponse(veh->m_vecMoveSpeed, travel);
			if(push.moved){
				float plen = sqrtf(push.dx * push.dx + push.dy * push.dy + push.dz * push.dz);
				if(plen > 1e-6f)
					VehicleVelocityResponse(veh->m_vecMoveSpeed, CVector(-push.dx / plen, -push.dy / plen, -push.dz / plen));
				if(push.onTop)
					veh->m_vecMoveSpeed.z = 0.0f;
			}
			pos += total;
			veh->SetPosition(pos);
		}
		newPrev[veh] = pos;
	}
	ms_prevPos.swap(newPrev);
}

void
Update(Mc::World &world)
{
	int minX, minY, minZ, maxX, maxY, maxZ;
	if(!world.GetBounds(minX, minY, minZ, maxX, maxY, maxZ))
		return;
	UpdatePeds(world, minX, minY, minZ, maxX, maxY, maxZ);
	UpdateVehicles(world, minX, minY, minZ, maxX, maxY, maxZ);
}

}

#endif
