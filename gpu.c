#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "gbemu.h"

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 144

extern settings_t settings;
extern mem_t mem;
extern cpu_t cpu;
extern gpu_t gpu;

SDL_Surface *surface;

void initDisplay() {
	SDL_Color c[4];
	
	if(SDL_Init(SDL_INIT_EVERYTHING) == -1) {
		printf("Failed to initialize SDL.\n");
		exit(1);
	}
	
	surface = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 8, SDL_HWPALETTE);
	if(!surface) {
		printf("Failed to create a window.\n");
		exit(1);
	}
	
	SDL_WM_SetCaption("GB Emulator", "GB Emulator");
	
	// SDL Palette
	c[0].r = c[0].g = c[0].b = 255;
	c[1].r = c[1].g = c[1].b = 192;
	c[2].r = c[2].g = c[2].b = 96;
	c[3].r = c[3].g = c[3].b = 0;
	SDL_SetPalette(surface, SDL_LOGPAL|SDL_PHYSPAL, c, 0, 4);
}

void initGPU() {
	int i, j;
	
	gpu.mode = GPU_MODE_HBLANK;
	gpu.mclock = 0;
	gpu.r.line = 0;
	gpu.r.scx = 0;
	gpu.r.scy = 0;
	gpu.r.flags = 0;

	gpu.vram = (uint8_t *)calloc(0x2000, sizeof(uint8_t));
	gpu.oam = (uint8_t *)calloc(0xA0, sizeof(uint8_t));
	
	gpu.tileset = malloc(0x200 * sizeof(uint8_t **));
	for(i=0; i<512; i++) {
		gpu.tileset[i] = malloc(8 * sizeof(uint8_t *));
		for(j=0; j<8; j++) {
			gpu.tileset[i][j] = calloc(8, sizeof(uint8_t));
		}
	}
}

void freeGPU() {
	int i, j;
	
	free(gpu.vram);
	free(gpu.oam);
	
	for(i=0; i<512; i++) {
		for(j=0; j<8; j++) {
			free(gpu.tileset[i][j]);
		}
		free(gpu.tileset[i]);
	}
	free(gpu.tileset);	
}

uint8_t gpuReadIOByte(uint16_t addr) {
	uint8_t res;
	
	switch(addr) {
		case 0:
			return gpu.r.flags;
			
		case 2:
			return gpu.r.scy;
			
		case 3:
			return gpu.r.scx;
			
		case 4:
			return gpu.r.line;
			
		default:
			return 0x00; // TODO: Unhandled I/O, no idea what GB does here
	}
}

void gpuWriteIOByte(uint8_t b, uint16_t addr) {
	uint8_t i;
	
	switch(addr) {
		case 0:
			gpu.r.flags = b;
			break;
		
		case 2:
			gpu.r.scy = b;
			break;
			
		case 3:
			gpu.r.scx = b;
			break;
			
		case 7:
			for(i=0; i<4; i++) {
				gpu.palette[i] = (b >> (i * 2)) & 3;
			}
			break;
			
		default:
			// Do nothing
			break;
	}
}

inline void setPixelColor(int x, int y, uint8_t color) {
	//printf("Setting (%d,%d) to (%d).\n", x, y, color);
	((uint8_t*)surface->pixels)[y*SCREEN_WIDTH + x] = color;
}

void printGPUDebug() {
	printf("bgtoggle=%d,lcdtoggle=%d,bgmap=%d,tileset=%d,scx=%d,scy=%d\n",
		(gpu.r.flags | GPU_FLAG_BG) ? 1 : 0, 
		(gpu.r.flags | GPU_FLAG_LCD) ? 1 : 0,
		(gpu.r.flags | GPU_FLAG_TILEMAP) ? 1 : 0,
		(gpu.r.flags | GPU_FLAG_TILESET) ? 1 : 0,
		gpu.r.scx, gpu.r.scy);

	
	int base = 0x1800;
	int i;
	
	for(i=base; i<=base+0x3FF; i++) {
		if(i%32 == 0) printf("\n");
		printf("%02X ", gpu.vram[i]);
	}
	printf("\n");
}

void renderScanline() {
	uint16_t yoff, xoff, tilenr;
	uint8_t row, col, i;
	uint8_t color;
	int bit, offset;
	
	// Use tilemap 1 or tilemap 0?
	yoff = (gpu.r.flags | GPU_FLAG_TILEMAP) ? GPU_TILEMAP_ADDR1 : GPU_TILEMAP_ADDR0;
	
	// divide y offset by 8 (to get whole tile)
	// then multiply by 32 (= amount of tiles in a row)
	yoff += (((gpu.r.line + gpu.r.scy) & 255) >> 3) << 5;
	
	// divide x offset by 8 (to get whole tile)
	xoff = gpu.r.scx >> 3;
	
	// row number inside our tile, we only need 3 bits
	row = (gpu.r.line + gpu.r.scy) & 0x07;
	
	// same with column number
	col = gpu.r.scx & 0x07;
	
	// tile number from bgmap
	tilenr = gpu.vram[yoff + xoff];

	// TODO: tilemap signed stuff
	
	for(i=0; i<160; i++) {
		color = gpu.palette[gpu.tileset[tilenr][row][col]];
		
		/*
		bit = 1 << (7 - col);
		offset = tilenr * 16 + row * 2;
		color = gpu.palette[((gpu.vram[offset] & bit) ? 0x01 : 0x00)
							| ((gpu.vram[offset+1] & bit) ? 0x02 : 0x00)];
		*/
		setPixelColor(i, gpu.r.line, color);
		
		col++;
		if(col == 8) {
			col = 0;
			xoff = (xoff + 1) & 0x1F;
			tilenr = gpu.vram[yoff + xoff];
		}
	}
	
}

void updateTile(uint8_t b, uint16_t addr) {
	int tile, row, col, bit;
	
	tile = (addr >> 4) & 0x1FF;
	row = (addr >> 1) & 0x07;
	
	//printf("updateTile! tile=%d, row=%d.\n", tile, row);
	
	for(col=0; col < 8; col++) {
		bit = 1 << (7 - col);
		gpu.tileset[tile][row][col] = ((gpu.vram[addr] & bit) ? 1 : 0);
		gpu.tileset[tile][row][col] |= ((gpu.vram[addr+1] & bit) ? 2 : 0);
	}
}

void stepGPU() {
	gpu.mclock += cpu.dc;
	switch(gpu.mode) {
		case GPU_MODE_HBLANK:
			if(gpu.mclock >= 51) {
				gpu.mclock = 0;
				gpu.r.line++;
				
				if(gpu.r.line == 144) { // last line
					gpu.mode = GPU_MODE_VBLANK;
					SDL_Flip(surface);
				}
				else {
					gpu.mode = GPU_MODE_OAM;
				}
			}
			break;
			
		case GPU_MODE_VBLANK:
			if(gpu.mclock >= 114) {
				gpu.mclock = 0;
				gpu.r.line++;
				
				if(gpu.r.line > 153) {
					gpu.mode = GPU_MODE_OAM;
					gpu.r.line = 0;
				}
			}
			break;
			
		case GPU_MODE_OAM:
			if(gpu.mclock >= 20) {
				gpu.mclock = 0;
				gpu.mode = GPU_MODE_VRAM;
			}
			break;
			
		case GPU_MODE_VRAM:
			if(gpu.mclock >= 43) {
				gpu.mclock = 0;
				gpu.mode = GPU_MODE_HBLANK;
				renderScanline();
			}
	}
}
