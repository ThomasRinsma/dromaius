#ifndef INCLUDED_GUI_H
#define INCLUDED_GUI_H

#include <cstdint>
#include "gbemu.h"

struct GUI
{
	// window states
	bool showDebugCPU = true;
	bool showDebugGraphics = true;
	bool showDebugAudio = true;

	~GUI();
	void initialize();
	void render();
};

#endif // include guard