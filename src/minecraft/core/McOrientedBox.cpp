#include "McOrientedBox.h"
#include <math.h>

namespace Mc {

static const float EPS = 1e-4f;

struct Hit {
	bool overlap;
	float depth;
	int axis;	// 0 X, 1 Y, 2 u, 3 v, 4 Z
	float sign;	// push direction along the axis
};

// SAT between the box and one cell: world X, Y, the box's u and v (horizontal), and Z.
static Hit TestCell(const OrientedBox &b, int cx, int cy, int cz)
{
	Hit best = { false, 1e30f, -1, 1.0f };
	float c = cosf(b.yaw), s = sinf(b.yaw);
	float ax[4] = { 1.0f, 0.0f, c, -s };
	float ay[4] = { 0.0f, 1.0f, s, c };
	float cellX = (float)cx + 0.5f, cellY = (float)cy + 0.5f;

	for(int i = 0; i < 4; i++){
		float uDot = c * ax[i] + s * ay[i];		// u . a
		float vDot = -s * ax[i] + c * ay[i];		// v . a
		float r = b.halfU * fabsf(uDot) + b.halfV * fabsf(vDot);
		float cellR = 0.5f * (fabsf(ax[i]) + fabsf(ay[i]));
		float d = (b.x * ax[i] + b.y * ay[i]) - (cellX * ax[i] + cellY * ay[i]);
		float overlap = r + cellR - fabsf(d);
		if(overlap <= EPS)
			return best;			// separated on this axis
		if(overlap < best.depth){
			best.depth = overlap;
			best.axis = i;
			best.sign = d >= 0.0f ? 1.0f : -1.0f;
		}
	}
	float mid = 0.5f * (b.zBottom + b.zTop), half = 0.5f * (b.zTop - b.zBottom);
	float d = mid - ((float)cz + 0.5f);
	float overlap = half + 0.5f - fabsf(d);
	if(overlap <= EPS)
		return best;
	if(overlap < best.depth){
		best.depth = overlap;
		best.axis = 4;
		best.sign = d >= 0.0f ? 1.0f : -1.0f;
	}
	best.overlap = true;
	return best;
}

static void CellRange(const OrientedBox &b, int &x0, int &y0, int &z0, int &x1, int &y1, int &z1)
{
	float c = fabsf(cosf(b.yaw)), s = fabsf(sinf(b.yaw));
	float rx = c * b.halfU + s * b.halfV;
	float ry = s * b.halfU + c * b.halfV;
	x0 = (int)floorf(b.x - rx + EPS);
	x1 = (int)floorf(b.x + rx - EPS);
	y0 = (int)floorf(b.y - ry + EPS);
	y1 = (int)floorf(b.y + ry - EPS);
	z0 = (int)floorf(b.zBottom + EPS);
	z1 = (int)floorf(b.zTop - EPS);
}

bool BoxOverlapsBlocks(const World &w, const OrientedBox &b)
{
	int x0, y0, z0, x1, y1, z1;
	CellRange(b, x0, y0, z0, x1, y1, z1);
	for(int z = z0; z <= z1; z++)
	for(int y = y0; y <= y1; y++)
	for(int x = x0; x <= x1; x++)
		if(w.Get(x, y, z) != BLOCK_AIR && TestCell(b, x, y, z).overlap)
			return true;
	return false;
}

BoxPush PushBoxOutOfBlocks(const World &w, OrientedBox &b)
{
	BoxPush res = { false, false, 0.0f, 0.0f, 0.0f };
	for(int iter = 0; iter < 8; iter++){
		int x0, y0, z0, x1, y1, z1;
		CellRange(b, x0, y0, z0, x1, y1, z1);
		bool pushed = false;
		for(int z = z0; z <= z1 && !pushed; z++)
		for(int y = y0; y <= y1 && !pushed; y++)
		for(int x = x0; x <= x1 && !pushed; x++){
			if(w.Get(x, y, z) == BLOCK_AIR)
				continue;
			Hit h = TestCell(b, x, y, z);
			if(!h.overlap)
				continue;
			float c = cosf(b.yaw), s = sinf(b.yaw);
			float dx = 0.0f, dy = 0.0f, dz = 0.0f;
			float m = h.depth * h.sign;
			switch(h.axis){
			case 0: dx = m; break;
			case 1: dy = m; break;
			case 2: dx = c * m; dy = s * m; break;
			case 3: dx = -s * m; dy = c * m; break;
			default: dz = m; break;
			}
			b.x += dx; b.y += dy;
			b.zBottom += dz; b.zTop += dz;
			res.dx += dx; res.dy += dy; res.dz += dz;
			res.moved = true;
			if(h.axis == 4 && h.sign > 0.0f)
				res.onTop = true;
			pushed = true;
		}
		if(!pushed)
			break;
	}
	return res;
}

SweepResult SweepBox(const World &w, const OrientedBox &from, float dx, float dy, float dz)
{
	SweepResult res = { false, from.x + dx, from.y + dy, dz };
	if(BoxOverlapsBlocks(w, from))
		return res;
	float dist = sqrtf(dx * dx + dy * dy + dz * dz);
	int steps = (int)ceilf(dist / 0.4f);
	if(steps < 1)
		return res;
	if(steps > 1000)
		steps = 1000;
	for(int i = 1; i <= steps; i++){
		float t = (float)i / (float)steps;
		OrientedBox b = from;
		b.x += dx * t;
		b.y += dy * t;
		b.zBottom += dz * t;
		b.zTop += dz * t;
		if(BoxOverlapsBlocks(w, b)){
			float p = (float)(i - 1) / (float)steps;
			res.blocked = true;
			res.x = from.x + dx * p;
			res.y = from.y + dy * p;
			res.dz = dz * p;
			return res;
		}
	}
	return res;
}

}
