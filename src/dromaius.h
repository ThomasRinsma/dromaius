#ifndef INCLUDED_DROMAIUS_H
#define INCLUDED_DROMAIUS_H

#include <cstdint>
#include <string>
#include <imgui.h>
#include <imgui_internal.h>
#include <imfilebrowser.h>
#include <GL/gl3w.h>
#include <SDL2/SDL.h>

#include "audio.h"
#include "cpu.h"
#include "graphics.h"
#include "gui.h"
#include "input.h"
#include "memory.h"
#include "games/games.h"

typedef struct keymap_s {
	int start;
	int startDown;
	
	int select;
	int selectDown;
	
	int left;
	int leftDown;
	
	int up;
	int upDown;
	
	int right;
	int rightDown;
	
	int down;
	int downDown;
	
	int b;
	int bDown;
	
	int a;
	int aDown;
} keymap_t;


typedef struct settings_s {
	int debug;
	keymap_t keymap;
} settings_t;


struct Dromaius
{
	// GB subcomponents
	CPU cpu;
	Graphics graphics;
	Input input;
	Memory memory;
	Audio audio;

	// Emulator subcomponents
	GUI gui;
	settings_t settings;

	// State
	std::string filename;

	Dromaius(settings_t settings);

	bool initializeWithRom(std::string const filename);
	void unloadRom();
	void reset();
	void initSettings();

	void saveState(uint8_t slot);
	bool loadState(uint8_t slot);
	
	void run();
};



#define HEADER_START    0x134

#define TILEMAP_ADDR0   0x1800
#define TILEMAP_ADDR1   0x1C00

#define CPU_CLOCKS_PER_FRAME 17556 // 70224 / 4 clock cycles

// Stubs for function definitions
struct SDL_Window;
typedef union SDL_Event SDL_Event;

// GUI stuff
IMGUI_IMPL_API bool ImGui_ImplSDL2_InitForOpenGL(SDL_Window* window, void* sdl_gl_context);
IMGUI_IMPL_API bool ImGui_ImplSDL2_InitForVulkan(SDL_Window* window);
IMGUI_IMPL_API bool ImGui_ImplSDL2_InitForD3D(SDL_Window* window);
IMGUI_IMPL_API void ImGui_ImplSDL2_Shutdown();
IMGUI_IMPL_API void ImGui_ImplSDL2_NewFrame(SDL_Window* window);
IMGUI_IMPL_API bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event);

// Set default OpenGL3 loader to be gl3w
#if !defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)     \
 && !defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)     \
 && !defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)     \
 && !defined(IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
#define IMGUI_IMPL_OPENGL_LOADER_GL3W
#endif

IMGUI_IMPL_API bool ImGui_ImplOpenGL3_Init(const char* glsl_version = NULL);
IMGUI_IMPL_API void ImGui_ImplOpenGL3_Shutdown();
IMGUI_IMPL_API void ImGui_ImplOpenGL3_NewFrame();
IMGUI_IMPL_API void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data);

// Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool ImGui_ImplOpenGL3_CreateFontsTexture();
IMGUI_IMPL_API void ImGui_ImplOpenGL3_DestroyFontsTexture();
IMGUI_IMPL_API bool ImGui_ImplOpenGL3_CreateDeviceObjects();
IMGUI_IMPL_API void ImGui_ImplOpenGL3_DestroyDeviceObjects();



#endif
