#ifndef INCLUDED_GBEMU_H
#define INCLUDED_GBEMU_H

#include <cstdint>
#include <SDL2/SDL.h>

#define HEADER_START    0x134

#define TILEMAP_ADDR0   0x1800
#define TILEMAP_ADDR1   0x1C00

#define CPU_CLOCKS_PER_FRAME 17556 // 70224 / 4 clock cycles

typedef struct keymap_s {
	int start;
	int startDown;
	
	int select;
	int selectDown;
	
	int left;
	int leftDown;
	
	int up;
	int upDown;
	
	int right;
	int rightDown;
	
	int down;
	int downDown;
	
	int b;
	int bDown;
	
	int a;
	int aDown;
} keymap_t;

typedef struct settings_s {
	int debug;
	keymap_t keymap;
} settings_t;


#endif
