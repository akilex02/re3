#include "mctest.h"
#include "McMesher.h"

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
