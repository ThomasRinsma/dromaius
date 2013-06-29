#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "gbemu.h"

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 144
#define WINDOW_SCALE  3

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
	
	surface = SDL_SetVideoMode(WINDOW_SCALE * SCREEN_WIDTH, WINDOW_SCALE * SCREEN_HEIGHT, 8, SDL_HWPALETTE);
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
	
	gpu.tileset = malloc(0x200 * sizeof(uint8_t **)); // = 512 dec
	for(i=0; i<512; i++) {
		gpu.tileset[i] = malloc(8 * sizeof(uint8_t *));
		for(j=0; j<8; j++) {
			gpu.tileset[i][j] = calloc(8, sizeof(uint8_t));
		}
	}
	
	gpu.spritedata = malloc(0x28 * sizeof(sprite_t)); // = 40 dec
	for(i=0; i<40; i++) {
		gpu.spritedata[i].x = -8;
		gpu.spritedata[i].y = -16;
		gpu.spritedata[i].tile = 0;
		gpu.spritedata[i].flags = 0;
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
	
	free(gpu.spritedata);
}

uint8_t gpuReadIOByte(uint16_t addr) {
	uint8_t res;
	
	switch(addr) {
		case 0:
			return gpu.r.flags;

		case 1: // LCD status
			//printf("  gpu.r.line = %d, gpu.r.lineComp = %d.\n", gpu.r.line, gpu.r.lineComp);
			return (gpu.mode & 0x03) +
				(gpu.r.line == gpu.r.lineComp ? 0x04 : 0x00) +
				(gpu.hBlankInt ? 0x08 : 0x00) +
				(gpu.vBlankInt ? 0x10 : 0x00) +
				(gpu.OAMInt ? 0x20 : 0x00) +
				(gpu.CoinInt ? 0x40 : 0x00);

		case 2:
			return gpu.r.scy;
			
		case 3:
			return gpu.r.scx;
			
		case 4:
			return gpu.r.line;

		case 5:
			return gpu.r.lineComp;
			
		default:
			printf("TODO! read from unimplemented 0x%02X\n", addr + 0xFF40);
			return 0x00; // TODO: Unhandled I/O, no idea what GB does here
	}
}

void gpuWriteIOByte(uint8_t b, uint16_t addr) {
	uint8_t i;
	
	switch(addr) {
		case 0:
			gpu.r.flags = b;
			break;

		case 1: // LCD status, writing is only allowed on the interrupt enable flags
			gpu.hBlankInt = (b & 0x08 ? 1 : 0);
			gpu.vBlankInt = (b & 0x10 ? 1 : 0);	
			gpu.OAMInt = (b & 0x20 ? 1 : 0);	
			gpu.CoinInt = (b & 0x40 ? 1 : 0);
			printf("written 0x%02X to lcd STAT\n", b);
			break;
		
		case 2:
			gpu.r.scy = b;
			break;
			
		case 3:
			gpu.r.scx = b;
			break;

		case 4:
			printf("written to read-only 0xFF44!\n");
			break;

		case 5:
			gpu.r.lineComp = b;
			printf("written %d to lineComp\n", b);
			break;
			
		case 6: // DMA from XX00-XX9F to FE00-FE9F
			for(i=0; i<=0x9F; i++) {
				writeByte(readByte((b << 8) + i), 0xFE00 + i);
			}
			break;
			
		case 7: // Background palette
			for(i=0; i<4; i++) {
				gpu.bgpalette[i] = (b >> (i * 2)) & 3;
			}
			break;
			
		case 8: // Object palette 0
			for(i=0; i<4; i++) {
				gpu.objpalette[0][i] = (b >> (i * 2)) & 3;
			}
			break;
			
		case 9: // Object palette 1
			for(i=0; i<4; i++) {
				gpu.objpalette[1][i] = (b >> (i * 2)) & 3;
			}
			break;
			
		default:
			printf("TODO! write to unimplemented 0x%02X\n", addr + 0xFF40);
			break;
	}
}

inline void setPixelColor(int x, int y, uint8_t color) {
	int i, j, drawx, drawy;
	
	if(x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || x < 0 || y < 0) {
		return;
	}
	
	for(i=0; i<WINDOW_SCALE; i++) {
		for(j=0; j<WINDOW_SCALE; j++) {
			drawx = WINDOW_SCALE * x + i;
			drawy = WINDOW_SCALE * y + j;
			
			((uint8_t*)surface->pixels)[drawy*(SCREEN_WIDTH * WINDOW_SCALE) + drawx] = color;
		}
	}

	
}

void printGPUDebug() {
	printf("bgtoggle=%d,spritetoggle=%d,lcdtoggle=%d,bgmap=%d,tileset=%d,scx=%d,scy=%d\n",
		(gpu.r.flags & GPU_FLAG_BG) ? 1 : 0, 
		(gpu.r.flags & GPU_FLAG_SPRITES) ? 1 : 0, 
		(gpu.r.flags & GPU_FLAG_LCD) ? 1 : 0,
		(gpu.r.flags & GPU_FLAG_TILEMAP) ? 1 : 0,
		(gpu.r.flags & GPU_FLAG_TILESET) ? 1 : 0,
		gpu.r.scx, gpu.r.scy);
		
	printf("gpu.spritedata[0].x = 0x%02X.\n", gpu.spritedata[0].x);
	printf("0xC000: %02X %02X %02X %02X.\n", readByte(0xC000), readByte(0xC001), readByte(0xC002), readByte(0xC003));
	printf("0xFE00: %02X %02X %02X %02X.\n", readByte(0xFE00), readByte(0xFE01), readByte(0xFE02), readByte(0xFE03));
	/*
	int base = 0x1800;
	int i;
	
	for(i=base; i<=base+0x3FF; i++) {
		if(i%32 == 0) printf("\n");
		printf("%02X ", gpu.vram[i]);
	}
	printf("\n");
	*/
}

void renderScanline() {
	uint16_t yoff, xoff, tilenr;
	uint8_t row, col, i, px;
	uint8_t color;
	uint8_t bgScanline[160];
	int bit, offset;

	
	if(gpu.r.flags & GPU_FLAG_BG) {
		// Use tilemap 1 or tilemap 0?
		yoff = (gpu.r.flags & GPU_FLAG_TILEMAP) ? GPU_TILEMAP_ADDR1 : GPU_TILEMAP_ADDR0;
	
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
			bgScanline[i] = gpu.tileset[tilenr][row][col];
			color = gpu.bgpalette[gpu.tileset[tilenr][row][col]];
		
			/*
			bit = 1 << (7 - col);
			offset = tilenr * 16 + row * 2;
			color = gpu.bgpalette[((gpu.vram[offset] & bit) ? 0x01 : 0x00)
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
	
	if(gpu.r.flags & GPU_FLAG_SPRITES) {
		for(i=0; i < 40; i++) {
			// do we need to draw the sprite?
			//printf("sprite x: %d, sprite y: %d, gpu line: %d\n", gpu.spritedata[i].x, gpu.spritedata[i].y, gpu.r.line);
			if(gpu.spritedata[i].y <= gpu.r.line && gpu.spritedata[i].y > gpu.r.line - 8) {
				// determine row, flip y if wanted
				if(gpu.spritedata[i].flags & SPRITE_FLAG_YFLIP) {
					row = 7 - (gpu.r.line - gpu.spritedata[i].y);
				} else {
					row = gpu.r.line - gpu.spritedata[i].y;
				}
				
				// loop through the collumns
				for(col=0; col<8; col++) {
					if(gpu.spritedata[i].flags & SPRITE_FLAG_XFLIP) {
						px = gpu.spritedata[i].x + (7 - col);
					} else {
						px = gpu.spritedata[i].x + col;
					}
					
					// only draw if this pixel's on the screen
					if(px >= 0 && px < 160) {
						// only draw pixel when color is not 0 and (sprite has priority or background pixel is 0)
						if(gpu.tileset[gpu.spritedata[i].tile][row][col] != 0 && (!(gpu.spritedata[i].flags & SPRITE_FLAG_PRIORITY) || bgScanline[px] == 0)) {
							color = gpu.objpalette[(gpu.spritedata[i].flags & SPRITE_FLAG_PALETTE) ? 1 : 0]
									[gpu.tileset[gpu.spritedata[i].tile][row][col]];
									                  
							setPixelColor(px, gpu.r.line, color);
						}
					}
				}
			}
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

void gpuBuildSpriteData(uint8_t b, uint16_t addr) {
	uint16_t spriteNum = addr >> 2;
	
	if(spriteNum < 40) { // Only 40 sprites
		switch(addr & 0x03) {
			case 0: // Y-coord
				gpu.spritedata[spriteNum].y = b - 16;
				//printf("Sprite #%d to Y of %d.\n", spriteNum, b-16);
				break;
				
			case 1: // X-coord
				gpu.spritedata[spriteNum].x = b - 8;
				//printf("Sprite #%d to X of %d.\n", spriteNum, b-8);
				break;
			
			case 2: // Tile
				gpu.spritedata[spriteNum].tile = b;
				break;
				
			case 3: // Flags
				gpu.spritedata[spriteNum].flags = b;
				break;
		}
	}
}

void stepGPU() {
	gpu.mclock += cpu.dc;
	switch(gpu.mode) {
		case GPU_MODE_HBLANK:
			if(gpu.mclock >= 51) {
				gpu.mclock = 0;
				gpu.r.line++;

				//printf("coinInt = %d, line = %d, lineComp = %d\n", gpu.CoinInt, gpu.r.line, gpu.r.lineComp);
				if(gpu.CoinInt && gpu.r.line == gpu.r.lineComp) {
						cpu.intFlags |= INT_LCDSTAT;
				}

				
				if(gpu.r.line == 144) { // last line
					gpu.mode = GPU_MODE_VBLANK;
					SDL_Flip(surface);
					cpu.intFlags |= INT_VBLANK;

					if(gpu.vBlankInt) {
						cpu.intFlags |= INT_LCDSTAT;
					}
				}
				else {
					gpu.mode = GPU_MODE_OAM;

					if(gpu.OAMInt) {
						cpu.intFlags |= INT_LCDSTAT;
					}
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

					if(gpu.OAMInt) {
						cpu.intFlags |= INT_LCDSTAT;
					}
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

				if(gpu.hBlankInt) {
					cpu.intFlags |= INT_LCDSTAT;
				}
			}
	}
}
