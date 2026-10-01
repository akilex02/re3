#pragma once

#include "McWorld.h"

namespace McMode
{
	void Init(void);
	void Shutdown(void);
	void Update(void);
	void Render(void);
	void Render2d(void);
	bool IsActive(void);
	Mc::World &GetWorld(void);
}
