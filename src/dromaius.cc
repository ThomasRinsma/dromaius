#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "dromaius.h"


// Give all components an up-reference to us.
Dromaius::Dromaius(settings_t settings)
{
	cpu.emu = this;
	graphics.emu = this;
	input.emu = this;
	memory.emu = this;
	audio.emu = this;
	gui.emu = this;

	// Save the settings
	this->settings = settings;
}

bool Dromaius::initializeWithRom(std::string const filename)
{
	// Save the ROM filename
	this->filename = filename;

	// Reset and re-initialize state
	reset();

	// Load the ROM from file into memory
	return memory.loadRom(this->filename);
}

void Dromaius::reset()
{
	// (re-)initialize GB components
	cpu.initialize();
	graphics.initialize();
	input.initialize();
	memory.initialize();
	audio.initialize();
}

void Dromaius::run()
{
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
			graphics.renderDebugTileset();
			
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
			ImGui_ImplSDL2_ProcessEvent(&event);
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
						reset();
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
}