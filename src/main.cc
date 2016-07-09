#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "gbemu.h"

#include "audio.h"
#include "cpu.h"
#include "graphics.h"
#include "input.h"
#include "memory.h"

// Global structs
Audio audio;
CPU cpu;
Graphics graphics;
Input input;
Memory memory;

settings_t settings;

void resetEmulation(std::string const &filename)
{
	graphics.freeBuffers();
	memory.freeBuffers();

	memory.initialize();
	memory.loadRom(filename);
	cpu.initialize();
	graphics.initialize();
	audio.initialize();
	input.initialize();
}

int main(int argc, char *argv[])
{
	SDL_Event event;
	romheader_t *romheader;

	if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
		std::cout << "Failed to initialize SDL.\n";
		exit(1);
	}
	
	settings.debug = 0;
	settings.keymap.start = SDLK_RETURN;
	settings.keymap.select = SDLK_RSHIFT;
	settings.keymap.left = SDLK_LEFT;
	settings.keymap.up = SDLK_UP;
	settings.keymap.right = SDLK_RIGHT;
	settings.keymap.down = SDLK_DOWN;
	settings.keymap.b = SDLK_z;
	settings.keymap.a = SDLK_x;
	
	// Load ROM file
	if (argc > 1 && memory.loadRom(argv[1])) {
		std::cout << "Succesfully loaded ROM '" << argv[1]
		          << "' of size " << memory.romLen << " into memory.\n";
	} else {
		std::cerr << "Error loading rom, exiting.\n";
		exit(1);
	}

	// TODO: move to Memory
	// Read the ROM header
	romheader = (romheader_t *)(&memory.rom[HEADER_START]);
	printf("CGB: 0x%02X, SGB: 0x%02X, OLIC: 0x%02X, NLIC: 0x%04X, country: 0x%02X\n",
		romheader->colorbyte, romheader->sgbfeatures, romheader->oldlicensee,
		romheader->newlicensee, romheader->country);
	printf("type: 0x%02X, romsize: 0x%02X, ramsize: 0x%02X\n\n",
		romheader->type, romheader->romsize, romheader->ramsize);

	// Set ram size (2/8/32 KByte)
	memory.ramSize = romheader->ramsize;

	// Set MBC type
	if (romheader->type == 0x00) {
		memory.mbc = Memory::MBC::NONE;
	} else if (romheader->type < 0x05) {
		memory.mbc = Memory::MBC::MBC1;
	} else if (romheader->type == 0x05 || romheader->type == 0x06) {
		memory.mbc = Memory::MBC::MBC2;
	} else if (romheader->type >= 0x0F && romheader->type < 0x15) {
		memory.mbc = Memory::MBC::MBC3;
	} else if (romheader->type >= 0x15 && romheader->type < 0x19) {
		memory.mbc = Memory::MBC::MBC4;
	} else if (romheader->type >= 0x19 && romheader->type < 0x1E) {
		memory.mbc = Memory::MBC::MBC5;
	} else {
		memory.mbc = Memory::MBC::OTHER;
	}

	graphics.initDisplay();
	
	// Instruction loop
	bool done = false;
	while (not done) {
		// Do a frame
		int oldTime = SDL_GetTicks();
		int frametime = cpu.c + CPU_CLOCKS_PER_FRAME;
		while (cpu.c < frametime) {
			if (not cpu.executeInstruction()) {
				done = 1;
				break;
			}
			if (settings.debug) {
				//printRegisters();
			}
			graphics.step();
		}

		graphics.renderDebugBackground();
		
		// SDL event loop
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
					case SDLK_F1: // toggle: debugging on every instruction
						settings.debug = !settings.debug;
						break;
						
					case SDLK_F2: // debug Graphics
						graphics.printDebug();
						cpu.printRegisters();
						break;
							
					case SDLK_F3: // dump memory contents to file
						memory.dumpToFile("memdump.bin");
						break;
						
					case SDLK_F4:
						printf("buttons held: 0x%02X, buttons down: 0x%02X. scroll timer: 0x%02X\n",
							memory.readByte(0xC0A1), memory.readByte(0xC0A2), memory.readByte(0xC0A3));
						break;
					
					case SDLK_r: // reset
						resetEmulation(argv[1]);
						break;
					
					default:
						input.handleGameInput(0, event.key.keysym.sym);
						break;
				}
			}
			else if (event.type == SDL_KEYUP) {
				input.handleGameInput(1, event.key.keysym.sym);
			}
			else if (event.type == SDL_QUIT ||
				(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) {
				done = true;
			}
		}
		
		uint32_t deltaTime = SDL_GetTicks() - oldTime;
		if (deltaTime < 16) {
			SDL_Delay(16 - deltaTime);
		}
	}

	SDL_Quit();
	return 0;
}


