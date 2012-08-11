#include <stdio.h>
#include <stdlib.h>
#include "gbemu.h"

extern settings_t settings;
extern mem_t mem;
extern cpu_t cpu;
extern gpu_t gpu;

// Hardcoded gameboy bios
uint8_t biosBytes[] = {
	0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E,
	0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
	0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
	0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
	0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
	0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
	0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
	0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
	0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xF2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
	0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
	0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
	0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
	0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
	0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3c, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x4C,
	0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
	0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50
};

void initMemory(uint8_t *rombuffer, size_t romlen) {
	mem.biosLoaded = 1;
	mem.bios = biosBytes;
	
	// This changes the BIOS to jump over logo check
	mem.bios[0x00E0] = 0xC3; // JP nn
	mem.bios[0x00E1] = 0x00; // 0x0100 (low byte)
	mem.bios[0x00E2] = 0x01; // 0x0100 (high byte)
	
	mem.inputWire = 0x10;
	mem.inputRow[0] = 0x0F;
	mem.inputRow[1] = 0x0F;
	
	mem.rom = rombuffer;
	mem.romlen = romlen;
	
	mem.workram = (uint8_t *)calloc(0x2000, sizeof(uint8_t));
	mem.extram = (uint8_t *)calloc(0x2000, sizeof(uint8_t));
	mem.zeropageram = (uint8_t *)calloc(127, sizeof(uint8_t));
}

void freeMemory() {
	free(mem.rom);
	free(mem.workram);
	free(mem.extram);
	free(mem.zeropageram);
}

// Read a byte from memory
uint8_t readByte(uint16_t addr) {
	switch(addr & 0xF000) {
		// BIOS / ROM0
		case 0x0000:
			if(mem.biosLoaded) {
				if(addr < 0x0100) return mem.bios[addr];
				else if(addr == 0x0100) {
					printf("End of bios reached!\n");
					mem.biosLoaded = 0;
				}
			}
			return mem.rom[addr];
		
		// ROM0
		case 0x1000:
		case 0x2000:
		case 0x3000:
			return mem.rom[addr];
			
		// ROM1, no bank
		case 0x4000:
		case 0x5000:
		case 0x6000:
		case 0x7000:
			return mem.rom[addr];
			
		// VRAM
		case 0x8000:
		case 0x9000:
			return gpu.vram[addr & 0x1FFF];
			
		// External RAM
		case 0xA000:
		case 0xB000:
			return mem.extram[addr & 0x1FFF];
			
		// Working RAM
		case 0xC000:
		case 0xD000:
		case 0xE000:
			return mem.workram[addr & 0x1FFF];
			
		case 0xF000:
			if(addr < 0xFE00) { // Working RAM shadow
				return mem.workram[addr & 0x1FFF];
			}
			
			if((addr & 0x0F00) == 0x0E00) {
				if(addr < 0xFEA0) { // Object Attribute Memory
			    	return gpu.oam[addr & 0xFF];
			    }
			    else {
			    	return 0;
			    }
			}
			else {
				if(addr == 0xFFFF) { // interrupt enable
					return cpu.ints;
				}
				else if(addr >= 0xFF80) { // Zero page
					return mem.zeropageram[addr & 0x7F];
				}
				else if(addr >= 0xFF40) { // I/O
					return gpuReadIOByte(addr - 0xFF40);
				}
				else if(addr == 0xFF0F) { // interrupt flags
					return cpu.intFlags;
				}
				else if(addr == 0xFF00) { 
					if(mem.inputWire == 0x10) {
						return mem.inputRow[0];
					}
					if(mem.inputWire == 0x20) {
						return mem.inputRow[1];
					}
					return 0;
				}
				
				return 0;
			}
		
	}
	printf("Memory error! 0x%02X\n", addr);
}

uint16_t readWord(uint16_t addr) {
	return readByte(addr) + (readByte(addr+1) << 8);
}

void writeByte(uint8_t b, uint16_t addr) {
	switch(addr & 0xF000) {
		// BIOS / ROM0
		case 0x0000:
			if(mem.biosLoaded) {
				if(addr < 0x0100) {
					mem.bios[addr] = b;
					return;
				}
				else if(addr == 0x0100) {
					mem.biosLoaded = 0;
				}
			}
			
			mem.rom[addr] = b;
			return;
		
		// ROM0
		case 0x1000:
		case 0x2000:
		case 0x3000:
			mem.rom[addr] = b;
			return;
			
		// ROM1, no bank
		case 0x4000:
		case 0x5000:
		case 0x6000:
		case 0x7000:
			mem.rom[addr] = b;
			return;
			
		// VRAM
		case 0x8000:
		case 0x9000:
			gpu.vram[addr & 0x1FFF] = b;
			updateTile(b, addr & 0x1FFE);
			return;
			
		// External RAM
		case 0xA000:
		case 0xB000:
			mem.extram[addr & 0x1FFF] = b;
			return;
			
		// Working RAM
		case 0xC000:
		case 0xD000:
		case 0xE000:
			mem.workram[addr & 0x1FFF] = b;
			return;
			
		case 0xF000:
			if(addr < 0xFE00) { // Working RAM shadow
				mem.workram[addr & 0x1FFF] = b;
				return;
			}
			if((addr & 0x0F00) == 0x0E00) {
				if(addr < 0xFEA0) { // Object Attribute Memory
			    	gpu.oam[addr & 0xFF] = b;
			    	gpuBuildSpriteData(b, addr - 0xFE00);
			    	return;
			    }
			    else {
			    	// writes here do nothing
			    	return;
			    }
			}
			else {
				if(addr == 0xFFFF) { // interrupt enable
					cpu.ints = b;
					return;
				}
				else if(addr >= 0xFF80) { // Zero page
					mem.zeropageram[addr & 0x7F] = b;
					return;
				}
				else if(addr >= 0xFF40) { // I/O
					gpuWriteIOByte(b, addr - 0xFF40);
					return;
				}
				else if(addr == 0xFF0F) {
					cpu.intFlags = b; // TODO: Can we allow program to set these?
					return;
				}
				else if(addr == 0xFF00) {
					mem.inputWire = b & 0x30;
					return;
				}
				
			}
	}
}

void writeWord(uint16_t w, uint16_t addr) {
	writeByte(w & 0xFF, addr);
	writeByte(w >> 8, addr+1);
}

void handleGameInput(int state, SDLKey key) {
	if(state == 1) {
		// key up
		if(key == settings.keymap.start && settings.keymap.startDown) {
			mem.inputRow[0] |= 0x08;
			printf("start up!\n");
			settings.keymap.startDown = 0;
		}
		else if(key == settings.keymap.select && settings.keymap.selectDown) {
			mem.inputRow[0] |= 0x04;
			printf("select up!\n");
			settings.keymap.selectDown = 0;
		}
		else if(key == settings.keymap.b && settings.keymap.bDown) {
			mem.inputRow[0] |= 0x02;
			printf("b up!\n");
			settings.keymap.bDown = 0;
		}
		else if(key == settings.keymap.a && settings.keymap.aDown) {
			mem.inputRow[0] |= 0x01;
			printf("a up!\n");
			settings.keymap.aDown = 0;
		}
		
		else if(key == settings.keymap.down && settings.keymap.downDown) {
			mem.inputRow[1] |= 0x08;
			printf("down up!\n");
			settings.keymap.downDown = 0;
		}
		else if(key == settings.keymap.up && settings.keymap.upDown) {
			mem.inputRow[1] |= 0x04;
			printf("up up!\n");
			settings.keymap.upDown = 0;
		}
		else if(key == settings.keymap.left && settings.keymap.leftDown) {
			mem.inputRow[1] |= 0x02;
			printf("left up!\n");
			settings.keymap.leftDown = 0;
		}
		else if(key == settings.keymap.right && settings.keymap.rightDown) {
			mem.inputRow[1] |= 0x01;
			printf("right up!\n");
			settings.keymap.rightDown = 0;
		}
		
		
		


	} else {
		// key down
		if(key == settings.keymap.start && !settings.keymap.startDown) {
			mem.inputRow[0] &= ~0x08 & 0x0F;
			printf("start down!\n");
			settings.keymap.startDown = 1;
		}
		else if(key == settings.keymap.select && !settings.keymap.selectDown) {
			mem.inputRow[0] &= ~0x04 & 0x0F;
			printf("select down!\n");
			settings.keymap.selectDown = 1;
		}
		else if(key == settings.keymap.b && !settings.keymap.bDown) {
			mem.inputRow[0] &= ~0x02 & 0x0F;
			printf("b down!\n");
			settings.keymap.bDown = 1;
		}
		else if(key == settings.keymap.a && !settings.keymap.aDown) {
			mem.inputRow[0] &= ~0x01 & 0x0F;
			printf("a down!\n");
			settings.keymap.aDown = 1;
		}
		
		else if(key == settings.keymap.down && !settings.keymap.downDown) {
			mem.inputRow[1] &= ~0x08 & 0x0F;
			printf("down down!\n");
			settings.keymap.downDown = 1;
		}
		else if(key == settings.keymap.up && !settings.keymap.upDown) {
			mem.inputRow[1] &= ~0x04 & 0x0F;
			printf("up down!\n");
			settings.keymap.upDown = 1;
		}
		else if(key == settings.keymap.left && !settings.keymap.leftDown) {
			mem.inputRow[1] &= ~0x02 & 0x0F;
			printf("left down!\n");
			settings.keymap.leftDown = 1;
		}
		else if(key == settings.keymap.right && !settings.keymap.rightDown) {
			mem.inputRow[1] &= ~0x01 & 0x0F;
			printf("right down!\n");
			settings.keymap.rightDown = 1;
		}
		
		
		
	}
}

void dumpToFile() {
	FILE *f;
	int i;
	
	f = fopen("memdump.bin", "w");
	
	for(i=0; i<0xFFFF; i++) {
		fputc(readByte(i), f);
	}
	
	fclose(f);
}
