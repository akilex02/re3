#include "McWorld.h"
#include <string.h>

namespace Mc {

int FloorDiv(int a, int b)
{
	int q = a / b;
	if((a % b != 0) && ((a < 0) != (b < 0)))
		q--;
	return q;
}

int FloorMod(int a, int b)
{
	return a - FloorDiv(a, b) * b;
}

Chunk *World::FindChunk(ChunkPos p)
{
	ChunkMap::iterator it = m_chunks.find(p);
	return it == m_chunks.end() ? nullptr : it->second;
}

const Chunk *World::FindChunk(ChunkPos p) const
{
	ChunkMap::const_iterator it = m_chunks.find(p);
	return it == m_chunks.end() ? nullptr : it->second;
}

uint8_t World::Get(int x, int y, int z) const
{
	ChunkPos p = { FloorDiv(x, CHUNK_SIZE), FloorDiv(y, CHUNK_SIZE), FloorDiv(z, CHUNK_SIZE) };
	const Chunk *c = FindChunk(p);
	if(c == nullptr)
		return BLOCK_AIR;
	return c->blocks[ChunkIndex(FloorMod(x, CHUNK_SIZE), FloorMod(y, CHUNK_SIZE), FloorMod(z, CHUNK_SIZE))];
}

void World::MarkNeighbourDirty(int cx, int cy, int cz)
{
	ChunkPos p = { cx, cy, cz };
	Chunk *c = FindChunk(p);
	if(c)
		c->dirty = true;
}

bool World::Set(int x, int y, int z, uint8_t id)
{
	ChunkPos p = { FloorDiv(x, CHUNK_SIZE), FloorDiv(y, CHUNK_SIZE), FloorDiv(z, CHUNK_SIZE) };
	int lx = FloorMod(x, CHUNK_SIZE), ly = FloorMod(y, CHUNK_SIZE), lz = FloorMod(z, CHUNK_SIZE);
	Chunk *c = FindChunk(p);
	if(c == nullptr){
		if(id == BLOCK_AIR)
			return false;
		c = new Chunk;
		memset(c->blocks, 0, sizeof(c->blocks));
		c->count = 0;
		c->dirty = true;
		m_chunks[p] = c;
	}
	uint8_t &cell = c->blocks[ChunkIndex(lx, ly, lz)];
	if(cell == id)
		return false;
	if(cell == BLOCK_AIR)
		c->count++;
	if(id == BLOCK_AIR)
		c->count--;
	cell = id;
	c->dirty = true;

	if(lx == 0) MarkNeighbourDirty(p.x - 1, p.y, p.z);
	if(lx == CHUNK_SIZE - 1) MarkNeighbourDirty(p.x + 1, p.y, p.z);
	if(ly == 0) MarkNeighbourDirty(p.x, p.y - 1, p.z);
	if(ly == CHUNK_SIZE - 1) MarkNeighbourDirty(p.x, p.y + 1, p.z);
	if(lz == 0) MarkNeighbourDirty(p.x, p.y, p.z - 1);
	if(lz == CHUNK_SIZE - 1) MarkNeighbourDirty(p.x, p.y, p.z + 1);

	if(c->count == 0){
		delete c;
		m_chunks.erase(p);
	}
	return true;
}

void World::Clear()
{
	for(ChunkMap::iterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
		delete it->second;
	m_chunks.clear();
}

// Task 2 replaces these stubs.
bool World::Save(const char *) const { return false; }
bool World::Load(const char *) { return false; }

}
