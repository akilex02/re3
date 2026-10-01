#include "common.h"

#ifdef MINECRAFT_MODE

#include "main.h"
#include "McRenderer.h"
#include "McMesher.h"
#include "McAtlas.h"
#include <unordered_map>
#include <vector>

typedef std::unordered_map<Mc::ChunkPos, Mc::ChunkMesh, Mc::ChunkPosHash> MeshMap;
static MeshMap meshes;
static std::vector<RwIm3DVertex> vertexBuffer;
static std::vector<RwImVertexIndex> indexBuffer;
static std::vector<Mc::McVertex> batchVerts;
static std::vector<uint16_t> batchIdx;
static uint32 lastGeneration;
static RwTexture *lastTexture;

namespace McRenderer
{

// librw's im3d buffers hold only 10000 vertices/indices, so a chunk is drawn in batches of quads.
static void
DrawMesh(const Mc::ChunkMesh &m)
{
	if(m.idx.empty())
		return;
	size_t numQuads = m.verts.size() / 4;
	for(size_t q = 0; q < numQuads; q += Mc::MAX_QUADS_PER_DRAW){
		Mc::ExtractQuads(m, q, Mc::MAX_QUADS_PER_DRAW, batchVerts, batchIdx);
		vertexBuffer.resize(batchVerts.size());
		for(size_t i = 0; i < batchVerts.size(); i++){
			const Mc::McVertex &v = batchVerts[i];
			RwIm3DVertexSetPos(&vertexBuffer[i], v.x, v.y, v.z);
			RwIm3DVertexSetU(&vertexBuffer[i], v.u);
			RwIm3DVertexSetV(&vertexBuffer[i], v.v);
			RwIm3DVertexSetRGBA(&vertexBuffer[i], v.r, v.g, v.b, v.a);
		}
		indexBuffer.assign(batchIdx.begin(), batchIdx.end());
		if(RwIm3DTransform(vertexBuffer.data(), vertexBuffer.size(), nil, rwIM3D_VERTEXUV)){
			RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, indexBuffer.data(), indexBuffer.size());
			RwIm3DEnd();
		}
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
	// a new, changed or lost atlas texture means every chunk must be meshed again (flat colours or UVs)
	RwTexture *texture = McAtlas::GetTexture();
	if(McAtlas::Generation() != lastGeneration || texture != lastTexture){
		lastGeneration = McAtlas::Generation();
		lastTexture = texture;
		world.MarkAllDirty();
		meshes.clear();
	}
	bool textured = texture != nil;
	// rebuild dirty chunks
	for(Mc::World::ChunkMap::const_iterator it = world.Chunks().begin(); it != world.Chunks().end(); ++it){
		Mc::Chunk *c = it->second;
		if(c->dirty || meshes.find(it->first) == meshes.end()){
			Mc::MeshChunk(world, it->first, textured, meshes[it->first]);
			c->dirty = false;
		}
	}
	if(meshes.empty())
		return;

	// remember the render state so the rest of the frame is unaffected
	void *prevRaster;
	uint32 prevFilter, prevAddressU, prevAddressV;
	uint32 prevCull, prevFog, prevZWrite, prevZTest, prevVertexAlpha, prevSrcBlend, prevDestBlend;
	RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &prevRaster);
	RwRenderStateGet(rwRENDERSTATETEXTUREFILTER, &prevFilter);
	RwRenderStateGet(rwRENDERSTATETEXTUREADDRESSU, &prevAddressU);
	RwRenderStateGet(rwRENDERSTATETEXTUREADDRESSV, &prevAddressV);
	RwRenderStateGet(rwRENDERSTATECULLMODE, &prevCull);
	RwRenderStateGet(rwRENDERSTATEFOGENABLE, &prevFog);
	RwRenderStateGet(rwRENDERSTATEZWRITEENABLE, &prevZWrite);
	RwRenderStateGet(rwRENDERSTATEZTESTENABLE, &prevZTest);
	RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &prevVertexAlpha);
	RwRenderStateGet(rwRENDERSTATESRCBLEND, &prevSrcBlend);
	RwRenderStateGet(rwRENDERSTATEDESTBLEND, &prevDestBlend);

	// binding through the raster state ignores the texture's own filter and addressing, so set them here
	if(texture != nil){
		RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RwTextureGetRaster(texture));
		RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERNEAREST);
		RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSCLAMP);
	}else
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

	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, prevRaster);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)(uintptr)prevFilter);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESSU, (void*)(uintptr)prevAddressU);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESSV, (void*)(uintptr)prevAddressV);
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)(uintptr)prevZTest);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)(uintptr)prevZWrite);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)(uintptr)prevVertexAlpha);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)(uintptr)prevSrcBlend);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)(uintptr)prevDestBlend);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)(uintptr)prevCull);
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)(uintptr)prevFog);
}

void
Shutdown(void)
{
	meshes.clear();
}

}

#endif
