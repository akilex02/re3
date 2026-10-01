#include "mctest.h"
#include "McMesher.h"
#include "McAtlasData.h"

using namespace Mc;

static ChunkPos origin() { ChunkPos p = { 0, 0, 0 }; return p; }

MC_TEST(mesh_empty_chunk_is_empty)
{
	World w;
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 0);
	MC_CHECK_EQ(m.idx.size(), 0);
}

MC_TEST(mesh_single_block_has_six_faces)
{
	World w;
	w.Set(5, 5, 5, BLOCK_STONE);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 24);
	MC_CHECK_EQ(m.idx.size(), 36);
}

MC_TEST(mesh_two_adjacent_blocks_hide_shared_faces)
{
	World w;
	w.Set(5, 5, 5, BLOCK_STONE);
	w.Set(6, 5, 5, BLOCK_STONE);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 10 * 4);
}

MC_TEST(mesh_culls_against_block_in_neighbouring_chunk)
{
	World w;
	w.Set(15, 0, 0, BLOCK_STONE);   // chunk (0,0,0), east border
	w.Set(16, 0, 0, BLOCK_STONE);   // chunk (1,0,0)
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 5 * 4);
}

MC_TEST(mesh_glass_next_to_stone_keeps_stone_face_but_not_glass_face)
{
	World w;
	w.Set(5, 5, 5, BLOCK_STONE);
	w.Set(6, 5, 5, BLOCK_GLASS);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	// stone: 6 faces (glass is transparent, so the shared face stays); glass: 5 (stone hides one)
	MC_CHECK_EQ(m.verts.size(), 11 * 4);
}

MC_TEST(mesh_glass_next_to_glass_hides_shared_faces)
{
	World w;
	w.Set(5, 5, 5, BLOCK_GLASS);
	w.Set(6, 5, 5, BLOCK_GLASS);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.verts.size(), 10 * 4);
}

MC_TEST(mesh_quads_face_outward_with_ccw_winding)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK_EQ(m.idx.size(), 36);
	for(size_t t = 0; t + 2 < m.idx.size(); t += 3){
		const McVertex &a = m.verts[m.idx[t]], &b = m.verts[m.idx[t + 1]], &c = m.verts[m.idx[t + 2]];
		float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
		float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
		float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
		// centre of the block is (0.5,0.5,0.5); the face normal must point away from it
		float cx = (a.x + b.x + c.x) / 3 - 0.5f, cy = (a.y + b.y + c.y) / 3 - 0.5f, cz = (a.z + b.z + c.z) / 3 - 0.5f;
		MC_CHECK(nx * cx + ny * cy + nz * cz > 0.0f);
	}
}

MC_TEST(mesh_uses_absolute_world_coordinates_including_negative_chunks)
{
	World w;
	w.Set(-1, -1, -1, BLOCK_DIRT);
	ChunkMesh m;
	ChunkPos p = { -1, -1, -1 };
	MeshChunk(w, p, false, m);
	MC_CHECK_EQ(m.verts.size(), 24);
	float minx = 1e9f, maxx = -1e9f;
	for(size_t i = 0; i < m.verts.size(); i++){
		if(m.verts[i].x < minx) minx = m.verts[i].x;
		if(m.verts[i].x > maxx) maxx = m.verts[i].x;
	}
	MC_CHECK_NEAR(minx, -1.0, 1e-5);
	MC_CHECK_NEAR(maxx, 0.0, 1e-5);
}

MC_TEST(mesh_flat_colour_vs_textured)
{
	World w;
	w.Set(0, 0, 0, BLOCK_STONE);
	ChunkMesh flat, tex;
	MeshChunk(w, origin(), false, flat);
	MeshChunk(w, origin(), true, tex);
	// find a top-face vertex (all four vertices of the quad have z == 1.0f)
	bool found = false;
	for(size_t q = 0; q * 4 + 3 < flat.verts.size(); q++){
		size_t idx = q * 4;
		if(flat.verts[idx].z == 1.0f &&
		   flat.verts[idx+1].z == 1.0f &&
		   flat.verts[idx+2].z == 1.0f &&
		   flat.verts[idx+3].z == 1.0f){
			MC_CHECK_EQ(flat.verts[idx].r, 125);   // stone colour x shade 1.0
			MC_CHECK_EQ(tex.verts[idx].r, 255);    // white x shade 1.0
			found = true;
			break;
		}
	}
	MC_CHECK(found);
}

MC_TEST(mesh_worst_case_checkerboard_fits_16_bit_indices)
{
	World w;
	for(int z = 0; z < 16; z++)
		for(int y = 0; y < 16; y++)
			for(int x = 0; x < 16; x++)
				if((x + y + z) % 2 == 0)
					w.Set(x, y, z, BLOCK_STONE);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK(m.verts.size() <= 65535);
	MC_CHECK(m.verts.size() > 40000);
}

static void buildHollowBox(World &w)
{
	for(int z = 0; z < 16; z++)
		for(int y = 0; y < 16; y++)
			for(int x = 0; x < 16; x++){
				bool shell = x == 0 || x == 15 || y == 0 || y == 15 || z == 0 || z == 15;
				if(shell)
					w.Set(x, y, z, BLOCK_STONE);
			}
}

MC_TEST(extract_quads_batches_cover_big_mesh_within_im3d_limits)
{
	World w;
	buildHollowBox(w);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	MC_CHECK(m.verts.size() > 10000);	// would overflow librw's im3d buffers in one draw
	size_t quads = m.verts.size() / 4;
	MC_CHECK_EQ(m.idx.size(), quads * 6);

	size_t covered = 0;
	for(size_t q = 0; q < quads; q += MAX_QUADS_PER_DRAW){
		size_t n = quads - q < MAX_QUADS_PER_DRAW ? quads - q : MAX_QUADS_PER_DRAW;
		std::vector<McVertex> bv;
		std::vector<uint16_t> bi;
		ExtractQuads(m, q, n, bv, bi);
		MC_CHECK(bv.size() <= 6664);
		MC_CHECK(bi.size() <= 9996);
		MC_CHECK_EQ(bv.size(), n * 4);
		MC_CHECK_EQ(bi.size(), n * 6);
		for(size_t i = 0; i < bi.size(); i++)
			MC_CHECK(bi[i] < bv.size());
		covered += n;
	}
	MC_CHECK_EQ(covered, quads);
}

MC_TEST(extract_quads_concatenation_equals_original)
{
	World w;
	buildHollowBox(w);
	ChunkMesh m;
	MeshChunk(w, origin(), false, m);
	size_t quads = m.verts.size() / 4;
	std::vector<McVertex> all;
	for(size_t q = 0; q < quads; q += MAX_QUADS_PER_DRAW){
		size_t n = quads - q < MAX_QUADS_PER_DRAW ? quads - q : MAX_QUADS_PER_DRAW;
		std::vector<McVertex> bv;
		std::vector<uint16_t> bi;
		ExtractQuads(m, q, n, bv, bi);
		// indices rebased: batch-local index + 4*q equals the original index
		for(size_t i = 0; i < bi.size(); i++)
			MC_CHECK_EQ(bi[i] + 4 * q, m.idx[6 * q + i]);
		all.insert(all.end(), bv.begin(), bv.end());
	}
	MC_CHECK_EQ(all.size(), m.verts.size());
	for(size_t i = 0; i < all.size(); i++){
		MC_CHECK_NEAR(all[i].x, m.verts[i].x, 0.0);
		MC_CHECK_NEAR(all[i].y, m.verts[i].y, 0.0);
		MC_CHECK_NEAR(all[i].z, m.verts[i].z, 0.0);
		MC_CHECK_EQ(all[i].r, m.verts[i].r);
	}
}

MC_TEST(extract_quads_single_quad_is_identity)
{
	ChunkMesh m;
	McVertex v = { 0, 0, 0, 0, 0, 255, 255, 255, 255 };
	for(int i = 0; i < 4; i++){
		v.x = (float)i;
		m.verts.push_back(v);
	}
	static const uint16_t quad[6] = { 0, 1, 2, 0, 2, 3 };
	for(int i = 0; i < 6; i++)
		m.idx.push_back(quad[i]);
	std::vector<McVertex> bv;
	std::vector<uint16_t> bi;
	ExtractQuads(m, 0, 1, bv, bi);
	MC_CHECK_EQ(bv.size(), 4);
	MC_CHECK_EQ(bi.size(), 6);
	for(int i = 0; i < 4; i++)
		MC_CHECK_NEAR(bv[i].x, (float)i, 0.0);
	for(int i = 0; i < 6; i++)
		MC_CHECK_EQ(bi[i], quad[i]);
}

// A block's faces must stay inside its own tile of the ATLAS_TILES x ATLAS_TILES atlas, for ids in every row and column.
MC_TEST(mesh_textured_uvs_stay_inside_the_blocks_atlas_tile)
{
	const int ids[] = { BLOCK_DIRT, BLOCK_GLASS, 13, 14, 29 };
	for(size_t n = 0; n < sizeof(ids) / sizeof(ids[0]); n++){
		int id = ids[n];
		World w;
		w.Set(0, 0, 0, (uint8_t)id);
		ChunkMesh m;
		MeshChunk(w, origin(), true, m);
		MC_CHECK_EQ(m.verts.size(), 24);
		float tile = 1.0f / (float)ATLAS_TILES;
		float u0 = (float)(id % ATLAS_TILES) * tile, v0 = (float)(id / ATLAS_TILES) * tile;
		for(size_t i = 0; i < m.verts.size(); i++){
			MC_CHECK(m.verts[i].u >= u0 - 1e-5f && m.verts[i].u <= u0 + tile + 1e-5f);
			MC_CHECK(m.verts[i].v >= v0 - 1e-5f && m.verts[i].v <= v0 + tile + 1e-5f);
		}
	}
}
