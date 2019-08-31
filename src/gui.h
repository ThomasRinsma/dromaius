#ifndef INCLUDED_GUI_H
#define INCLUDED_GUI_H

#include <cstdint>
#include "gbemu.h"

struct GUI
{
	Graphics &graphics;

	void initialize(Graphics &graphics);
	~GUI();

	void render();
};

#endif // include guard