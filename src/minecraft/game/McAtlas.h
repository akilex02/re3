#pragma once

#ifdef MINECRAFT_MODE

// Block texture atlas built from Mojang's own block textures, downloaded once into mcassets/.
// All functions are called from the main thread; librw is never used by the download thread.
namespace McAtlas
{
	// Loads the atlas from mcassets/ when the cache is complete, otherwise starts the download thread
	// (at most once per launch).
	void Init(void);
	// Every frame: builds the atlas once the download thread has finished successfully.
	void Update(void);
	// The atlas texture (64x64, 4x4 tiles of 16x16, tile index = block id), or nil until ready.
	RwTexture *GetTexture(void);
	// 0 until the first atlas exists; incremented each time a new atlas texture becomes available.
	uint32 Generation(void);
	// Stops a running download at its next step (never waits for it) and releases the texture.
	void Shutdown(void);
}

#endif
