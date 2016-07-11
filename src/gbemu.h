#ifndef INCLUDED_GBEMU_H
#define INCLUDED_GBEMU_H

#include <cstdint>
#include <imgui.h>
#include <GL/gl3w.h>
#include <SDL2/SDL.h>

#define HEADER_START    0x134

#define TILEMAP_ADDR0   0x1800
#define TILEMAP_ADDR1   0x1C00

#define CPU_CLOCKS_PER_FRAME 17556 // 70224 / 4 clock cycles

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


IMGUI_API bool        ImGui_ImplSdlGL3_Init(SDL_Window* window);
IMGUI_API void        ImGui_ImplSdlGL3_Shutdown();
IMGUI_API void        ImGui_ImplSdlGL3_NewFrame(SDL_Window* window);
IMGUI_API bool        ImGui_ImplSdlGL3_ProcessEvent(SDL_Event* event);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void        ImGui_ImplSdlGL3_InvalidateDeviceObjects();
IMGUI_API bool        ImGui_ImplSdlGL3_CreateDeviceObjects();


#endif
