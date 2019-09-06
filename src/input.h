#ifndef INCLUDED_INPUT_H
#define INCLUDED_INPUT_H

#include <cstdint>
#include <SDL2/SDL.h>
struct Dromaius;
struct Input
{
	// Up-reference
	Dromaius *emu;

	uint8_t row[2];
	uint8_t wire;

	void initialize();

	void handleGameInput(int state, SDL_Keycode key);
};

#endif