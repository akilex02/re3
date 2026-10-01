#include "McCollide.h"
#include <math.h>

namespace Mc {

static const float EPS = 1e-4f;

CollideResult PushOutOfBlocks(const World &w, float &px, float &py, float &pz, float halfWidth, float height)
{
	CollideResult res = { false, false };
	for(int iter = 0; iter < 8; iter++){
		float lo[3] = { px - halfWidth, py - halfWidth, pz };
		float hi[3] = { px + halfWidth, py + halfWidth, pz + height };

		bool pushed = false;
		int c0[3], c1[3];
		for(int a = 0; a < 3; a++){
			c0[a] = (int)floorf(lo[a] + EPS);
			c1[a] = (int)floorf(hi[a] - EPS);
		}
		for(int z = c0[2]; z <= c1[2] && !pushed; z++)
		for(int y = c0[1]; y <= c1[1] && !pushed; y++)
		for(int x = c0[0]; x <= c1[0] && !pushed; x++){
			if(w.Get(x, y, z) == BLOCK_AIR)
				continue;
			int cell[3] = { x, y, z };
			// penetration along each axis, and the push direction that resolves it
			float best = 1e30f;
			int bestAxis = -1;
			float bestPush = 0.0f;
			for(int a = 0; a < 3; a++){
				float overlapPos = hi[a] - (float)cell[a];		// push the box toward -a
				float overlapNeg = (float)(cell[a] + 1) - lo[a];	// push the box toward +a
				if(overlapPos <= EPS || overlapNeg <= EPS)
					continue;	// no overlap on this axis
				float pen = overlapPos < overlapNeg ? overlapPos : overlapNeg;
				if(pen < best){
					best = pen;
					bestAxis = a;
					bestPush = overlapPos < overlapNeg ? -overlapPos : overlapNeg;
				}
			}
			if(bestAxis < 0)
				continue;
			if(bestAxis == 0) px += bestPush;
			else if(bestAxis == 1) py += bestPush;
			else pz += bestPush;
			res.moved = true;
			res.onGround = (bestAxis == 2 && bestPush > 0.0f);
			pushed = true;
		}
		if(!pushed)
			break;
	}
	return res;
}

}
