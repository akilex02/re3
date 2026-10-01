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

// librw's immediate-mode buffers hold 10000 vertices / 10000 indices; 1666 quads = 6664 verts / 9996 indices fit.
const size_t MAX_QUADS_PER_DRAW = 1666;

// Copy quads [firstQuad, firstQuad+quadCount) of the mesh (4 vertices + 6 indices each) into outVerts/outIdx,
// with the indices rebased to the extracted vertices. Out-of-range requests are clamped.
void ExtractQuads(const ChunkMesh &m, size_t firstQuad, size_t quadCount, std::vector<McVertex> &outVerts, std::vector<uint16_t> &outIdx);

void MeshChunk(const World &w, ChunkPos cp, bool textured, ChunkMesh &out);

}
