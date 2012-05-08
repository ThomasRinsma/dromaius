#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gbemu.h"

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

int main(int argc, char *argv[]) {
	int done;
	uint8_t *rombuffer;
	size_t romlen;
	SDL_Event event;
	
	// Open the rom file
	if(argc > 1 && readROMFromFile(argv[1], &rombuffer, &romlen)) {
		printf("Succesfully loaded ROM \"%s\" of size %d into memory.\n", argv[1], (int)romlen);
	} else {
		exit(1);
	}
	
	// Initialize stuff
	initCPU();
	initMemory(rombuffer, romlen);
	initGPU();
	initDisplay();
	
	// Instruction loop
	done = 0;
	while(!done) {
		if(!executeInstruction()) {
			break;
		}
		//printRegisters();
		stepGPU();
		
		
		// SDL event loop
		SDL_PollEvent(&event);
		if(event.type == SDL_KEYDOWN) {
			printGPUDebug();
			dumpToFile();
		}
		else if(event.type == SDL_QUIT) {
			done = 1;
		}
	}
	
	
	// TODO: Free things
	SDL_Quit();
	return 0;
}


