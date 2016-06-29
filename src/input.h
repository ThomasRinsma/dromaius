#ifndef INCLUDED_INPUT_H
#define INCLUDED_INPUT_H

#include <cstdint>
#include <SDL2/SDL.h>

struct Input
{

	uint8_t inputRow[2];
	uint8_t inputWire;

	void handleGameInput(int state, SDL_Keycode key);
};

#endif