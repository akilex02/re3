#include "common.h"

#ifdef MINECRAFT_MODE

#include "main.h"
#include "McRenderer.h"
#include "McMesher.h"
#include <unordered_map>
#include <vector>

typedef std::unordered_map<Mc::ChunkPos, Mc::ChunkMesh, Mc::ChunkPosHash> MeshMap;
static MeshMap meshes;
static std::vector<RwIm3DVertex> vertexBuffer;
static std::vector<RwImVertexIndex> indexBuffer;

namespace McRenderer
{

static void
DrawMesh(const Mc::ChunkMesh &m)
{
	if(m.idx.empty())
		return;
	vertexBuffer.resize(m.verts.size());
	for(size_t i = 0; i < m.verts.size(); i++){
		const Mc::McVertex &v = m.verts[i];
		RwIm3DVertexSetPos(&vertexBuffer[i], v.x, v.y, v.z);
		RwIm3DVertexSetU(&vertexBuffer[i], v.u);
		RwIm3DVertexSetV(&vertexBuffer[i], v.v);
		RwIm3DVertexSetRGBA(&vertexBuffer[i], v.r, v.g, v.b, v.a);
	}
	indexBuffer.assign(m.idx.begin(), m.idx.end());
	if(RwIm3DTransform(vertexBuffer.data(), vertexBuffer.size(), nil, rwIM3D_VERTEXUV)){
		RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, indexBuffer.data(), indexBuffer.size());
		RwIm3DEnd();
	}
}

void
Render(Mc::World &world)
{
	// drop meshes of freed chunks
	for(MeshMap::iterator it = meshes.begin(); it != meshes.end();){
		if(world.FindChunk(it->first) == nil)
			it = meshes.erase(it);
		else
			++it;
	}
	// rebuild dirty chunks
	for(Mc::World::ChunkMap::const_iterator it = world.Chunks().begin(); it != world.Chunks().end(); ++it){
		Mc::Chunk *c = it->second;
		if(c->dirty || meshes.find(it->first) == meshes.end()){
			Mc::MeshChunk(world, it->first, false, meshes[it->first]);
			c->dirty = false;
		}
	}
	if(meshes.empty())
		return;

	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)TRUE);

	for(MeshMap::const_iterator it = meshes.begin(); it != meshes.end(); ++it)
		DrawMesh(it->second);

	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLBACK);
}

void
Shutdown(void)
{
	meshes.clear();
}

}

#endif
