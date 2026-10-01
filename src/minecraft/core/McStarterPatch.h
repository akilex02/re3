#pragma once

#include "McWorld.h"

namespace Mc {

const int STARTER_PATCH_SIZE = 7;	// width and depth of the patch in blocks

// Builds a small survival patch (stone with coal and iron ore, a tree, sand and gravel) whose lowest
// corner is the cell (ox, oy, oz). It only writes inside [ox, ox+7) x [oy, oy+7) x [oz, oz+6).
// There is no terrain generator yet, so this is how the survival loop is tried out.
void BuildStarterPatch(World &world, int ox, int oy, int oz);

}
