#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gbemu.h"

settings_t settings;
cpu_t cpu;
gpu_t gpu;
mem_t mem;

int readROMFromFile(char *filename, uint8_t **buffer, size_t *romsize) {
	FILE *romfile;
	int read;
	
	// Open the file
	romfile = fopen(filename, "r");	

	if(!romfile) {
		printf("\"%s\" does not exist.\n", filename);
		return 0;
	}

	// Check the size and allocate accordingly
	fseek(romfile, 0, SEEK_END);
	*romsize = ftell(romfile);
	rewind(romfile);
	*buffer = (char *)malloc(*romsize * sizeof(char));
	if(*buffer == NULL) return 0;
	
	// Read the file into our buffer
	read = fread(*buffer, 1, *romsize, romfile);
	if(read != *romsize) return 0;
	
	// Close the file
	fclose(romfile);
	
	return 1;
}

void initEmulation(uint8_t *rombuffer, size_t romlen) {
	// Initialize stuff
	initCPU();
	//cpu.r.pc = 0x0100; // Jump over bios, not sure if anything important is happening in bios
	initMemory(rombuffer, romlen);
	initGPU();
}

int main(int argc, char *argv[]) {
	int done, frametime, oldTime, deltaTime;
	uint8_t *rombuffer;
	size_t romlen;
	SDL_Event event;
	
	settings.debug = 0;
	settings.keymap.start = SDLK_RETURN;
	settings.keymap.select = SDLK_RSHIFT;
	settings.keymap.left = SDLK_LEFT;
	settings.keymap.up = SDLK_UP;
	settings.keymap.right = SDLK_RIGHT;
	settings.keymap.down = SDLK_DOWN;
	settings.keymap.b = SDLK_z;
	settings.keymap.a = SDLK_x;
	
	// Open the rom file
	if(argc > 1 && readROMFromFile(argv[1], &rombuffer, &romlen)) {
		printf("Succesfully loaded ROM \"%s\" of size %d into memory.\n", argv[1], (int)romlen);
	} else {
		exit(1);
	}
	
	initEmulation(rombuffer, romlen);
	initDisplay();
	
	// Instruction loop
	done = 0;
	while(!done) {
		// Do a frame
		oldTime = SDL_GetTicks();
		frametime = cpu.c + CPU_CLOCKS_PER_FRAME;
		while(cpu.c < frametime) {
			if(!executeInstruction()) {
				done = 1;
				break;
			}
			//printRegisters();
			stepGPU();
		}
		
		// SDL event loop
		while(SDL_PollEvent(&event)) {;
			if(event.type == SDL_KEYDOWN) {
				switch(event.key.keysym.sym) {
					case SDLK_F1: // debug
						printGPUDebug();
						printRegisters();
						//dumpToFile();
						readByte(0x0100);
						settings.debug = 1;
						break;
					
					case SDLK_r: // reset
						freeGPU();
						freeMemory();
						readROMFromFile(argv[1], &rombuffer, &romlen);
						initEmulation(rombuffer, romlen);
						break;
					
					default:
						handleGameInput(0, event.key.keysym.sym);
						break;
				}
			}
			else if(event.type == SDL_KEYUP) {
				handleGameInput(1, event.key.keysym.sym);
			}
			else if(event.type == SDL_QUIT) {
				done = 1;
			}
		}
		
		deltaTime = SDL_GetTicks() - oldTime;
		
		if(deltaTime < 17) {
			SDL_Delay(16 - deltaTime);
		}
		
	}
	
	// Free things
	freeGPU();
	freeMemory();
	SDL_Quit();
	return 0;
}


