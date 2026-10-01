#include "McWorld.h"
#include <string.h>
#include <stdio.h>
#include <string>

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
	m_revision++;

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
	m_revision++;
}

bool World::Save(const char *path) const
{
	std::string tmp = std::string(path) + ".tmp";
	FILE *f = fopen(tmp.c_str(), "wb");
	if(f == nullptr)
		return false;
	uint32_t n = (uint32_t)m_chunks.size();
	bool ok = fwrite("MCW1", 1, 4, f) == 4 && fwrite(&n, sizeof(n), 1, f) == 1;
	for(ChunkMap::const_iterator it = m_chunks.begin(); ok && it != m_chunks.end(); ++it){
		int32_t pos[3] = { it->first.x, it->first.y, it->first.z };
		ok = fwrite(pos, sizeof(pos), 1, f) == 1 &&
			fwrite(it->second->blocks, 1, CHUNK_VOLUME, f) == (size_t)CHUNK_VOLUME;
	}
	if(fflush(f) != 0)
		ok = false;
	if(fclose(f) != 0)
		ok = false;
	if(ok){
#ifdef _WIN32
		// rename() does not replace an existing file on Windows. There is a tiny window between
		// remove and rename where neither file exists; the complete data is still in path.tmp.
		remove(path);
#endif
		ok = rename(tmp.c_str(), path) == 0;
	}
	if(!ok)
		remove(tmp.c_str());
	return ok;
}

bool World::Load(const char *path)
{
	Clear();
	FILE *f = fopen(path, "rb");
	if(f == nullptr)
		return false;
	char magic[4];
	uint32_t n = 0;
	bool ok = fread(magic, 1, 4, f) == 4 && memcmp(magic, "MCW1", 4) == 0 &&
		fread(&n, sizeof(n), 1, f) == 1 && n <= 1000000u;
	for(uint32_t i = 0; ok && i < n; i++){
		int32_t pos[3];
		Chunk *c = new Chunk;
		ok = fread(pos, sizeof(pos), 1, f) == 1 &&
			fread(c->blocks, 1, CHUNK_VOLUME, f) == (size_t)CHUNK_VOLUME;
		if(!ok){
			delete c;
			break;
		}
		c->count = 0;
		for(int j = 0; j < CHUNK_VOLUME; j++){
			if(c->blocks[j] >= BLOCK_COUNT)
				c->blocks[j] = BLOCK_AIR;	// unknown id from a damaged or newer file
			if(c->blocks[j] != BLOCK_AIR)
				c->count++;
		}
		c->dirty = true;
		ChunkPos p = { pos[0], pos[1], pos[2] };
		ChunkMap::iterator old = m_chunks.find(p);
		if(c->count == 0){
			delete c;
		}else{
			if(old != m_chunks.end())
				delete old->second;
			m_chunks[p] = c;
		}
	}
	fclose(f);
	if(!ok)
		Clear();
	m_revision++;
	return ok;
}

}
