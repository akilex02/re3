#pragma once

#include "McWorld.h"

namespace McRenderer
{
	// Rebuilds dirty chunk meshes (clearing their dirty flag) and draws all chunks.
	void Render(Mc::World &world);
	void Shutdown(void);
}
