#include "McRay.h"
#include <math.h>

namespace Mc {

RayHit RayCast(const World &w, float ox, float oy, float oz, float dx, float dy, float dz, float maxDist)
{
	RayHit miss = { false, 0, 0, 0, 0, 0, 0, 0.0f };
	const float o[3] = { ox, oy, oz };
	const float d[3] = { dx, dy, dz };
	int cell[3], step[3];
	float tMax[3], tDelta[3];
	const float inf = 1e30f;

	for(int a = 0; a < 3; a++){
		cell[a] = (int)floorf(o[a]);
		if(d[a] > 0.0f){
			step[a] = 1;
			tDelta[a] = 1.0f / d[a];
			tMax[a] = ((float)(cell[a] + 1) - o[a]) / d[a];
		}else if(d[a] < 0.0f){
			step[a] = -1;
			tDelta[a] = -1.0f / d[a];
			tMax[a] = (o[a] - (float)cell[a]) / -d[a];
		}else{
			step[a] = 0;
			tDelta[a] = inf;
			tMax[a] = inf;
		}
	}

	// Reject zero-direction rays and invalid maxDist (NaN, negative)
	if(step[0] == 0 && step[1] == 0 && step[2] == 0)
		return miss;
	if(!(maxDist >= 0.0f))
		return miss;

	// Clamp maxDist to prevent unbounded walks (callers use reach 6)
	float maxDistClamped = maxDist > 4096.0f ? 4096.0f : maxDist;

	float t = 0.0f;
	int n[3] = { 0, 0, 0 };
	for(;;){
		if(w.Get(cell[0], cell[1], cell[2]) != BLOCK_AIR){
			RayHit h = { true, cell[0], cell[1], cell[2], n[0], n[1], n[2], t };
			return h;
		}
		int a;
		if(tMax[0] < tMax[1])
			a = tMax[0] < tMax[2] ? 0 : 2;
		else
			a = tMax[1] < tMax[2] ? 1 : 2;
		if(tMax[a] > maxDistClamped)
			return miss;
		t = tMax[a];
		cell[a] += step[a];
		tMax[a] += tDelta[a];
		n[0] = n[1] = n[2] = 0;
		n[a] = -step[a];
	}
}

}
