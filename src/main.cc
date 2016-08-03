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
CPU cpu;
Graphics graphics;
Input input;
Memory memory;
Audio audio;

settings_t settings;

bool initEmulation(std::string const &filename)
{
	cpu.initialize();
	graphics.initialize();
	input.initialize();
	memory.initialize();
	audio.initialize();

	return memory.loadRom(filename);
}

void resetEmulation(std::string const &filename)
{
	graphics.freeBuffers();
	memory.freeBuffers();
	audio.freeBuffers();

	initEmulation(filename);
}

void initSettings()
{
	settings.debug = 0;
	settings.keymap.start = SDLK_RETURN;
	settings.keymap.select = SDLK_RSHIFT;
	settings.keymap.left = SDLK_LEFT;
	settings.keymap.up = SDLK_UP;
	settings.keymap.right = SDLK_RIGHT;
	settings.keymap.down = SDLK_DOWN;
	settings.keymap.b = SDLK_z;
	settings.keymap.a = SDLK_x;
}

int main(int argc, char *argv[])
{

	if (argc != 2) {
		std::cerr << "No rom provided, exiting.\n";
		exit(1);
	}

	// Try to initialize SDL.
	if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
		std::cerr << "Failed to initialize SDL.\n";
		exit(1);
	}

	// Initialize components and try to load ROM file.
	if (initEmulation(argv[1])) {
		std::cout << "Succesfully loaded ROM '" << argv[1]
		          << "' of size " << memory.romLen << " into memory.\n";
	} else {
		std::cerr << "Error loading rom, exiting.\n";
		exit(1);
	}

	// Create windows and buffers
	graphics.initDisplay();

	// Setup keymap and debug settings
	initSettings();
	
	// Instruction loop
	bool done = false;
	while (not done) {
		int oldTime = SDL_GetTicks();

		if (cpu.stepMode and cpu.stepInst) {
			// Perform one CPU instruction
			if (not cpu.executeInstruction()) {
				done = 1;
				break;
			}
			graphics.step();
			
			cpu.stepInst = false;
		} else if (not cpu.stepMode or cpu.stepFrame) {
			// Do a frame
			graphics.renderDebugBackground();
			
			int frametime = cpu.c + CPU_CLOCKS_PER_FRAME;
			while (cpu.c < frametime) {
				if (not cpu.executeInstruction()) {
					done = 1;
					break;
				}

				graphics.step();
			}

			cpu.stepFrame = false;
		}

		graphics.renderFrame();
		
		// SDL event loop
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSdlGL3_ProcessEvent(&event);
			ImGuiIO& io = ImGui::GetIO();

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

					case SDLK_SPACE:
						cpu.stepInst = true;
						break;

					case SDLK_f:
						cpu.stepFrame = true;
						break;
					
					default:
						if (not io.WantCaptureKeyboard) {
							input.handleGameInput(0, event.key.keysym.sym);
						}
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
		if (deltaTime < 16 and not cpu.fastForward) {
			SDL_Delay(16 - deltaTime);
		}
	}

	ImGui_ImplSdlGL3_Shutdown();
	//SDL_GL_DeleteContext(glcontext);
	//SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}


