#pragma once
#include <vector>
#include "McWorld.h"

namespace Mc {

struct McVertex {
	float x, y, z;
	float u, v;
	uint8_t r, g, b, a;
};

struct ChunkMesh {
	std::vector<McVertex> verts;
	std::vector<uint16_t> idx;
};

void MeshChunk(const World &w, ChunkPos cp, bool textured, ChunkMesh &out);

}
