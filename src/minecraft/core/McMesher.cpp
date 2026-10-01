#include "McMesher.h"
#include "McAtlasData.h"

namespace Mc {

// face = 0..5: +X, -X, +Y, -Y, +Z, -Z
static const float faceShade[3][2] = {
	{ 0.6f, 0.6f },	// X
	{ 0.8f, 0.8f },	// Y
	{ 1.0f, 0.5f },	// Z (top, bottom)
};

void MeshChunk(const World &w, ChunkPos cp, bool textured, ChunkMesh &out)
{
	out.verts.clear();
	out.idx.clear();
	const Chunk *c = w.FindChunk(cp);
	if(c == nullptr)
		return;

	for(int lz = 0; lz < CHUNK_SIZE; lz++)
	for(int ly = 0; ly < CHUNK_SIZE; ly++)
	for(int lx = 0; lx < CHUNK_SIZE; lx++){
		uint8_t id = c->blocks[ChunkIndex(lx, ly, lz)];
		if(id == BLOCK_AIR)
			continue;
		const BlockInfo &info = GetBlockInfo(id);
		int pos[3] = { cp.x * CHUNK_SIZE + lx, cp.y * CHUNK_SIZE + ly, cp.z * CHUNK_SIZE + lz };

		for(int face = 0; face < 6; face++){
			int a = face / 2;		// axis
			int s = (face % 2) ? -1 : 1;	// sign
			int nb[3] = { pos[0], pos[1], pos[2] };
			nb[a] += s;
			uint8_t nid = w.Get(nb[0], nb[1], nb[2]);
			if(nid != BLOCK_AIR && !(GetBlockInfo(nid).transparent && nid != id))
				continue;

			float shade = faceShade[a][face % 2];
			uint8_t r, g, b;
			if(textured){
				r = g = b = (uint8_t)(255.0f * shade);
			}else{
				r = (uint8_t)(info.r * shade);
				g = (uint8_t)(info.g * shade);
				b = (uint8_t)(info.b * shade);
			}
			uint8_t alpha = info.transparent ? 140 : 255;

			int ua = (a + 1) % 3, va = (a + 2) % 3;	// (a, ua, va) is cyclic, so ua x va = +a
			static const int cu[4] = { 0, 1, 1, 0 };
			static const int cv[4] = { 0, 0, 1, 1 };
			int order[4] = { 0, 1, 2, 3 };
			if(s < 0){
				order[1] = 3;
				order[3] = 1;	// reverse winding for negative faces
			}
			uint16_t base = (uint16_t)out.verts.size();
			for(int k = 0; k < 4; k++){
				int i = order[k];
				float p[3];
				p[a] = (float)(pos[a] + (s > 0 ? 1 : 0));
				p[ua] = (float)(pos[ua] + cu[i]);
				p[va] = (float)(pos[va] + cv[i]);

				float tu, tv;
				if(a == 2){ tu = (float)cu[i]; tv = (float)cv[i]; }
				else if(a == 0){ tu = (float)cu[i]; tv = 1.0f - cv[i]; }
				else { tu = (float)cv[i]; tv = 1.0f - cu[i]; }
				float tx = (float)(id % ATLAS_TILES), ty = (float)(id / ATLAS_TILES);

				McVertex v;
				v.x = p[0]; v.y = p[1]; v.z = p[2];
				v.u = (tx + tu) / (float)ATLAS_TILES;
				v.v = (ty + tv) / (float)ATLAS_TILES;
				v.r = r; v.g = g; v.b = b; v.a = alpha;
				out.verts.push_back(v);
			}
			static const uint16_t quad[6] = { 0, 1, 2, 0, 2, 3 };
			for(int k = 0; k < 6; k++)
				out.idx.push_back((uint16_t)(base + quad[k]));
		}
	}
}

void ExtractQuads(const ChunkMesh &m, size_t firstQuad, size_t quadCount, std::vector<McVertex> &outVerts, std::vector<uint16_t> &outIdx)
{
	outVerts.clear();
	outIdx.clear();
	size_t total = m.verts.size() / 4;
	if(firstQuad >= total)
		return;
	if(quadCount > total - firstQuad)
		quadCount = total - firstQuad;
	outVerts.assign(m.verts.begin() + 4 * firstQuad, m.verts.begin() + 4 * (firstQuad + quadCount));
	outIdx.reserve(quadCount * 6);
	for(size_t i = 6 * firstQuad; i < 6 * (firstQuad + quadCount); i++)
		outIdx.push_back((uint16_t)(m.idx[i] - 4 * firstQuad));
}

}
