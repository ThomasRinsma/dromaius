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
	0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C,
	0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
	0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50
};

void initMemory(uint8_t *rombuffer, size_t romlen) {
	mem.biosLoaded = 1;
	mem.bios = biosBytes;
	
	// This changes the BIOS to jump over logo check
	//mem.bios[0x00E0] = 0xC3; // JP nn
	//mem.bios[0x00E1] = 0x00; // 0x0100 (low byte)
	//mem.bios[0x00E2] = 0x01; // 0x0100 (high byte)
	
	mem.inputWire = 0x10;
	mem.inputRow[0] = 0x0F;
	mem.inputRow[1] = 0x0F;

	// debug, set right key as pressed
	//mem.inputRow[1] &= ~0x01 & 0x0F;
	
	mem.rom = rombuffer;
	mem.romlen = romlen;
	
	mem.workram = (uint8_t *)calloc(0x2000, sizeof(uint8_t));
	mem.extram = (uint8_t *)calloc(0x8000, sizeof(uint8_t)); // TODO allocate exact needed amount
	mem.zeropageram = (uint8_t *)calloc(127, sizeof(uint8_t));

	mem.ramEnabled = 0;
	mem.bankMode = 0;
	mem.ramBank = 0;
	mem.romBank = 1;
}

void freeMemory() {
	free(mem.rom);
	free(mem.workram);
	free(mem.extram);
	free(mem.zeropageram);
}

// Read a byte from memory
uint8_t readByte(uint16_t addr) {
	switch (addr & 0xF000) {
		// BIOS / ROM0
		case 0x0000:
			if (mem.biosLoaded) {
				if (addr < 0x0100) {
					return mem.bios[addr];
				}
				else if (addr == 0x0100) {
					//printf("End of bios reached!\n");
					mem.biosLoaded = 0;
				}
			}
			return mem.rom[addr];
		
		// ROM0
		case 0x1000:
		case 0x2000:
		case 0x3000:
			return mem.rom[addr];
			
		// ROM1, bank
		case 0x4000:
		case 0x5000:
		case 0x6000:
		case 0x7000:
			if (mem.mbc == MTYPE_NONE) {
				return mem.rom[addr];
			}
			else {
				// TODO: is this the same for all MBCs?
				return mem.rom[((mem.romBank - 1) * 0x4000) + addr];
			}
			
		// VRAM
		case 0x8000:
		case 0x9000:
			return gpu.vram[addr & 0x1FFF];
			
		// External RAM
		case 0xA000:
		case 0xB000:
			if (!mem.ramEnabled) {
				printf("Read from disabled external RAM.\n");
			}

			if (mem.mbc == MTYPE_NONE) {
				return mem.extram[addr & 0x1FFF];
			}
			else if (mem.mbc == MTYPE_MBC1) {
				if (mem.ramSize == 0x03) { // 4 banks of 8kb
					return mem.extram[(mem.ramBank * 0x2000) + (addr & 0x1FFF)];
				}
				else { // either 2KB or 8KB total, no banks
					return mem.extram[addr & 0x1FFF];
				}
			}
			else if (mem.mbc == MTYPE_MBC2) {
				if (addr <= 0xA1FF) {
					// TODO: not sure how to handle only using the lower nibble
					return mem.extram[addr & 0x1FFF] & 0x0F;
				}
				else {
					printf("Read from MBC2 RAM outside limit.\n");
				}
			}
			
			
		// Working RAM
		case 0xC000:
		case 0xD000:
		case 0xE000:
			return mem.workram[addr & 0x1FFF];
			
		case 0xF000:
			if (addr < 0xFE00) { // Working RAM shadow
				return mem.workram[addr & 0x1FFF];
			}
			
			if ((addr & 0x0F00) == 0x0E00) {
				if (addr < 0xFEA0) { // Object Attribute Memory
			    	return gpu.oam[addr & 0xFF];
			    }
			    else {
			    	return 0;
			    }
			}
			else {
				if (addr == 0xFFFF) { // interrupt enable
					return cpu.ints;
				}
				else if (addr >= 0xFF80) { // Zero page
					return mem.zeropageram[addr & 0x7F];
				}
				else if (addr >= 0xFF40) { // I/O
					return gpuReadIOByte(addr - 0xFF40);
				}
				else if (addr == 0xFF0F) { // interrupt flags
					return cpu.intFlags;
				}
				else if (addr == 0xFF04) {
					return cpu.timer.div;
				}
				else if (addr == 0xFF05) {
					return cpu.timer.tima;
				}
				else if (addr == 0xFF06) {
					return cpu.timer.tma;
				}
				else if (addr == 0xFF07) {
					return cpu.timer.tac;
				}
				else if (addr == 0xFF00) { 
					if (mem.inputWire == 0x10) {
						return mem.inputRow[0];
					}
					if (mem.inputWire == 0x20) {
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
		// Enable or disable external RAM
		case 0x0000:
		case 0x1000:
			if (mem.mbc == MTYPE_NONE) {
				printf("write to < 0x1FFFF without MBC\n");
			}
			else if (mem.mbc == MTYPE_MBC1) {
				mem.ramEnabled = ((b & 0xF) == 0xA);
			}
			else if (mem.mbc == MTYPE_MBC2) {
				// Least sign. bit of upper addr. byte should be 0
				if (!(addr & 0x0100)) {
					mem.ramEnabled = ((b & 0xF) == 0xA);
				}
				else {
					printf("MBC2 RAM enable with wrong bit.\n");
				}
			}
			break;

		// Set the lower 5 bits of ROM bank nr
		case 0x2000:
		case 0x3000: 
			if (mem.mbc == MTYPE_NONE) {
				printf("write to 0x2000-0x3FFF without MBC\n");
			}
			else if (mem.mbc == MTYPE_MBC1) {
				// Set lower 5 bits
				mem.romBank = (b & 0x1F) | (mem.romBank & 0xE0);

				// Apply rom bank glitch
				if (mem.romBank == 0x00 || mem.romBank == 0x20 ||
					mem.romBank == 0x40 || mem.romBank == 0x60) {
					mem.romBank++;
				}
			}
			else if (mem.mbc == MTYPE_MBC2) {
				// Least sign. bit of upper addr. byte should be 1
				if (addr & 0x0100) {
					// Only 16 banks for MBC2
					mem.romBank = (b & 0x0F);
				}
				else {
					printf("MBC2 ROM bank select with wrong bit.\n");
				}
			}
			return;
			
		// Set RAM bank nr or upper 2 bits of ROM bank nr
		case 0x4000:
		case 0x5000:
			if (mem.mbc == MTYPE_NONE) {
				printf("write to 0x4000-0x5FFF without MBC\n");
			}
			else {
				if (mem.bankMode) { // RAM
					mem.ramBank = (b & 0x3);
				}
				else { // ROM, upper 2 bits
					mem.romBank = (mem.romBank & 0x1F) | ((b & 0x3) << 5);

					// Apply rom bank glitch
					// TODO: only for MBC1 or also for 2?
					if (mem.romBank == 0x00 || mem.romBank == 0x20 ||
						mem.romBank == 0x40 || mem.romBank == 0x60) {
						mem.romBank++;
					}
				}
				
			}
			return;

		// ROM or RAM banking mode select
		case 0x6000:
		case 0x7000:
			if (mem.mbc == MTYPE_NONE) {
				printf("write to 0x6000-0x7FFF without MBC\n");
			}
			else {
				mem.bankMode = (b & 0x1);
			}
			return;
			
		// VRAM
		case 0x8000:
		case 0x9000:
			gpu.vram[addr & 0x1FFF] = b;
			//if (b == 0 && addr >= 0x9800 && addr < 0x9C00)
			//{
			//	printf("wrote b=%d to vram @ 0x%04X.\n", b, addr);

			//	printRegisters();
			//}
			updateTile(b, addr & 0x1FFE);
			return;
			
		// External RAM
		case 0xA000:
		case 0xB000:
			if (!mem.ramEnabled)
				printf("Write to disabled external RAM.\n");

			if (mem.mbc == MTYPE_NONE) {
				mem.extram[addr & 0x1FFF] = b;
			}
			else if (mem.mbc == MTYPE_MBC1) {
				if (mem.ramSize == 0x03) { // 4 banks of 8kb
					mem.extram[(mem.ramBank * 0x2000) + (addr & 0x1FFF)] = b;
				}
				else { // either 2KB or 8KB total, no banks
					mem.extram[addr & 0x1FFF] = b;
				}
			}
			else if (mem.mbc == MTYPE_MBC2) {
				if (addr <= 0xA1FF) {
					// TODO: not sure how to handle only using the lower nibble
					mem.extram[addr & 0x1FFF] = (b & 0x0F);
				}
				else {
					printf("Write to MBC2 RAM outside limit.\n");
				}
			}
			else {
				printf("0xA000-0xBFFF unimplemented else, TODO\n");
			}

			return;
			
		// Working RAM
		case 0xC000:
		case 0xD000:
		case 0xE000:
			mem.workram[addr & 0x1FFF] = b;
			return;
			
		case 0xF000:
			if (addr < 0xFE00) { // Working RAM shadow
				mem.workram[addr & 0x1FFF] = b;
				return;
			}
			if ((addr & 0x0F00) == 0x0E00) {
				if (addr < 0xFEA0) { // Object Attribute Memory
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
				if (addr == 0xFFFF) { // interrupt enable
					//printf("cpu.ints = %d\n", b);
					cpu.ints = b;
					return;
				}
				else if (addr >= 0xFF80) { // Zero page
					mem.zeropageram[addr & 0x7F] = b;
					return;
				}
				else if (addr >= 0xFF40) { // I/O
					gpuWriteIOByte(b, addr - 0xFF40);
					return;
				}
				else if (addr == 0xFF0F) {
					//printf("cpu.intFlags = %d\n", b);
					cpu.intFlags = b; // TODO: Can we allow program to set these?
					return;
				}
				else if (addr == 0xFF04) {
					cpu.timer.div = 0x00; // writing resets the timer
					return;
				}
				else if (addr == 0xFF05) {
					cpu.timer.tima = b;
					return;
				}
				else if (addr == 0xFF06) {
					cpu.timer.tma = b;
					return;
				}
				else if (addr == 0xFF07) {
					cpu.timer.tac = b;
					return;
				}
				else if (addr == 0xFF00) {
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
			settings.keymap.startDown = 0;
		}
		else if(key == settings.keymap.select && settings.keymap.selectDown) {
			mem.inputRow[0] |= 0x04;
			settings.keymap.selectDown = 0;
		}
		else if(key == settings.keymap.b && settings.keymap.bDown) {
			mem.inputRow[0] |= 0x02;
			settings.keymap.bDown = 0;
		}
		else if(key == settings.keymap.a && settings.keymap.aDown) {
			mem.inputRow[0] |= 0x01;
			settings.keymap.aDown = 0;
		}
		
		else if(key == settings.keymap.down && settings.keymap.downDown) {
			mem.inputRow[1] |= 0x08;
			settings.keymap.downDown = 0;
		}
		else if(key == settings.keymap.up && settings.keymap.upDown) {
			mem.inputRow[1] |= 0x04;
			settings.keymap.upDown = 0;
		}
		else if(key == settings.keymap.left && settings.keymap.leftDown) {
			mem.inputRow[1] |= 0x02;
			settings.keymap.leftDown = 0;
		}
		else if(key == settings.keymap.right && settings.keymap.rightDown) {
			mem.inputRow[1] |= 0x01;
			settings.keymap.rightDown = 0;
		}
	} else {
		// key down
		if(key == settings.keymap.start && !settings.keymap.startDown) {
			mem.inputRow[0] &= ~0x08 & 0x0F;
			settings.keymap.startDown = 1;
		}
		else if(key == settings.keymap.select && !settings.keymap.selectDown) {
			mem.inputRow[0] &= ~0x04 & 0x0F;
			settings.keymap.selectDown = 1;
		}
		else if(key == settings.keymap.b && !settings.keymap.bDown) {
			mem.inputRow[0] &= ~0x02 & 0x0F;
			settings.keymap.bDown = 1;
		}
		else if(key == settings.keymap.a && !settings.keymap.aDown) {
			mem.inputRow[0] &= ~0x01 & 0x0F;
			settings.keymap.aDown = 1;
		}
		
		else if(key == settings.keymap.down && !settings.keymap.downDown) {
			mem.inputRow[1] &= ~0x08 & 0x0F;
			settings.keymap.downDown = 1;
		}
		else if(key == settings.keymap.up && !settings.keymap.upDown) {
			mem.inputRow[1] &= ~0x04 & 0x0F;
			settings.keymap.upDown = 1;
		}
		else if(key == settings.keymap.left && !settings.keymap.leftDown) {
			mem.inputRow[1] &= ~0x02 & 0x0F;
			settings.keymap.leftDown = 1;
		}
		else if(key == settings.keymap.right && !settings.keymap.rightDown) {
			mem.inputRow[1] &= ~0x01 & 0x0F;
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
