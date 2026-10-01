#pragma once
#include <stdint.h>
#include <stddef.h>
#include <unordered_map>
#include "McBlocks.h"

namespace Mc {

const int CHUNK_SIZE = 16;
const int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

inline int ChunkIndex(int lx, int ly, int lz) { return lx + CHUNK_SIZE * (ly + CHUNK_SIZE * lz); }

// floor semantics for negative a; b must be > 0
int FloorDiv(int a, int b);
int FloorMod(int a, int b);

struct ChunkPos {
	int32_t x, y, z;
	bool operator==(const ChunkPos &o) const { return x == o.x && y == o.y && z == o.z; }
};

struct ChunkPosHash {
	size_t operator()(const ChunkPos &p) const
	{
		return (size_t)((uint32_t)p.x * 73856093u ^ (uint32_t)p.y * 19349663u ^ (uint32_t)p.z * 83492791u);
	}
};

struct Chunk {
	uint8_t blocks[CHUNK_VOLUME];
	int count;	// non-air blocks
	bool dirty;	// needs re-meshing
};

class World {
public:
	typedef std::unordered_map<ChunkPos, Chunk*, ChunkPosHash> ChunkMap;

	World() : m_revision(0) {}
	~World() { Clear(); }

	uint8_t Get(int x, int y, int z) const;
	// Returns true if the block changed. Setting air in a missing chunk is a no-op.
	bool Set(int x, int y, int z, uint8_t id);

	size_t ChunkCount() const { return m_chunks.size(); }
	Chunk *FindChunk(ChunkPos p);
	const Chunk *FindChunk(ChunkPos p) const;
	const ChunkMap &Chunks() const { return m_chunks; }
	void Clear();

	// Increments on every successful Set, Clear and Load; lets callers detect unsaved changes.
	uint32_t Revision() const { return m_revision; }

	// Save writes path.tmp and then replaces path, so a failed save leaves the old file intact.
	bool Save(const char *path) const;
	// Load of a valid file with zero chunks succeeds and leaves an empty world.
	bool Load(const char *path);

private:
	World(const World&);
	World &operator=(const World&);
	void MarkNeighbourDirty(int cx, int cy, int cz);

	ChunkMap m_chunks;
	uint32_t m_revision;
};

}
