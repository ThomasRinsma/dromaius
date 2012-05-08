#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "gbemu.h"

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 144

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
	gpu.line = 0;
	gpu.mclock = 0;

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

uint8_t gpuReadIOByte(uint16_t addr) {
	uint8_t res;
	
	switch(addr) {
		case 0:
			res  = (gpu.f.bgtoggle ? 0x01 : 0x00);
			res |= (gpu.f.bgmap ? 0x08 : 0x00);
			res |= (gpu.f.bgtile ? 0x10 : 0x00);
			res |= (gpu.f.lcdtoggle ? 0x80 : 0x00);
			return res;
			
		case 2:
			return gpu.f.scy;
			
		case 3:
			return gpu.f.scx;
			
		case 4:
			return gpu.line;
			
		default:
			return 0x00; // TODO: Unhandled I/O, no idea what GB does here
	}
}

void gpuWriteIOByte(uint8_t b, uint16_t addr) {
	uint8_t i;
	
	switch(addr) {
		case 0:
			gpu.f.bgtoggle = (b & 0x01) ? 1 : 0;
			gpu.f.bgmap = (b & 0x08) ? 1 : 0;
			gpu.f.bgtile = (b & 0x10) ? 1 : 0;
			gpu.f.lcdtoggle = (b & 0x80) ? 1 : 0;
			break;
		
		case 2:
			gpu.f.scy = b;
			
		case 3:
			gpu.f.scx = b;
			
		case 7:
			for(i=0; i<4; i++) {
				gpu.palette[i] = (b >> (i * 2)) & 3;
			}
	}
}

void setPixelColor(int x, int y, uint8_t color) {
	//printf("Setting (%d,%d) to (%d).\n", x, y, color);
	((uint8_t*)surface->pixels)[y*SCREEN_WIDTH + x] = color;
}

void printGPUDebug() {
	printf("bgtoggle=%d,lcdtoggle=%d,bgmap=%d,bgtile=%d,scx=%d,scy=%d\n",
			gpu.f.bgtoggle, gpu.f.lcdtoggle, gpu.f.bgmap, gpu.f.bgtile, gpu.f.scx, gpu.f.scy);

	
	int base = 0x1800;
	int i;
	
	for(i=base; i<=base+0x3FF; i++) {
		if(i%32 == 0) printf("\n");
		printf("%02X ", gpu.vram[i]);
	}
	printf("\n");
	exit(1);
}

void renderScanline() {
	uint16_t yoff, xoff, tilenr;
	uint8_t row, col, i;
	uint8_t color;
	int bit, offset;
	
	yoff = gpu.f.bgmap ? 0x1C00 : 0x1800;
	yoff += (((gpu.line + gpu.f.scy) & 255) >> 3) << 5;
	xoff = gpu.f.scx >> 3;
	
	row = (gpu.line + gpu.f.scy) & 0x07;
	col = gpu.f.scx & 0x07;
	
	tilenr = gpu.vram[yoff + xoff];

	//printf("line=%d, yoff=0x%02X, xoff=0x%02X, yoff+xoff=0x%02X, tilenr=0x%02X\n", gpu.line, yoff, xoff, yoff+xoff, tilenr);
	for(i=0; i<160; i++) {
		color = gpu.palette[gpu.tileset[tilenr][row][col]];
		
		/*
		bit = 1 << (7 - col);
		offset = tilenr * 16 + row * 2;
		color = gpu.palette[((gpu.vram[offset] & bit) ? 0x01 : 0x00)
							| ((gpu.vram[offset+1] & bit) ? 0x02 : 0x00)];
		*/
		setPixelColor(i, gpu.line, color);
		
		col++;
		if(col == 8) {
			col = 0;
			xoff = (xoff + 1) & 0x1F;
			tilenr = gpu.vram[yoff + xoff];
		}
	}
	
}

void updateDisplay() {

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
				gpu.line++;
				
				if(gpu.line == 144) { // last line
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
				gpu.line++;
				
				if(gpu.line > 153) {
					gpu.mode = GPU_MODE_OAM;
					gpu.line = 0;
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
