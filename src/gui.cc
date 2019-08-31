#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "gbemu.h"

#define GUI_INDENT_WIDTH 16.0f

void GUI::initialize() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	// Setup ImGui Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(graphics.mainWindow, graphics.glcontext);
	ImGui_ImplOpenGL3_Init(graphics.glsl_version);
}

GUI::~GUI() {

}

void GUI::render() {
	// Set global style
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;

	// Scale main window up to SDL window size
	int windowWidth, windowHeight;
	SDL_GetWindowSize(graphics.mainWindow, &windowWidth, &windowHeight);
	ImGui::SetNextWindowPos(ImVec2(.0f, .0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::Begin("Main window", nullptr, 
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		// ImGuiWindowFlags_NoMouseInputs |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_MenuBar
	);

	// Menubar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem("Open...");
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) {
				printf("TODO exit\n");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug windows"))
		{
			ImGui::MenuItem("CPU info", nullptr, &showDebugCPU);
			ImGui::MenuItem("Graphics info", nullptr, &showDebugGraphics);
			ImGui::MenuItem("Audio info", nullptr, &showDebugAudio);
			
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	// Info window
	ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoResize);
		ImGui::Text("ROM Name: %s\nMBC: %s\nCountry: %s\nROM size: %d\nRAM size: %d",
			memory.romheader->gamename,
			memory.mbcAsString().c_str(),
			memory.romheader->country ? "Other" : "Japan",
			memory.romheader->romsize,
			memory.romheader->ramsize);
	ImGui::End(); // info window


	// Settings window
	ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoResize);
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		if (ImGui::Button("Reset ROM")) {
			// hacky, sorry
			SDL_Event resetEvent;
			resetEvent.type = SDL_KEYDOWN;
			resetEvent.key.keysym.sym = SDLK_r;
			SDL_PushEvent(&resetEvent);
		}
		if (ImGui::Button("Dump memory to \nfile (memdump.bin)")) {
			memory.dumpToFile("memdump.bin");
		}
		//ImGui::SliderInt("Scale", &screenScale, 1, 8);
		ImGui::Checkbox("Fast forward", &cpu.fastForward);
		ImGui::Checkbox("Step mode", &cpu.stepMode);
		if (ImGui::Button("Step instruction (space)")) {
			cpu.stepInst = true;
		}
		if (ImGui::Button("Step frame (f)")) {
			cpu.stepFrame = true;
		}
	ImGui::End();


	// CPU Debug window
	if (showDebugCPU) {
		ImGui::Begin("CPU", nullptr, ImGuiWindowFlags_NoResize);
		ImGui::Text("cycle: %d", cpu.c);
		if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text(
					"a: %02X       hl: %04X\n"
					"b: %02X\n"
					"c: %02X       pc: %04X\n"
					"d: %02X       sp: %04X\n"
					"e: %02X   \n"
					"f: %c,%c,%c,%c",
				cpu.r.a, (cpu.r.h << 8) | cpu.r.l, cpu.r.b, cpu.r.c,
				cpu.r.pc, cpu.r.d, cpu.r.sp, cpu.r.e,
				cpu.getFlag(CPU::Flag::ZERO) ? 'Z' : '_',
				cpu.getFlag(CPU::Flag::SUBTRACT) ? 'N' : '_',
				cpu.getFlag(CPU::Flag::HCARRY) ? 'H' : '_',
				cpu.getFlag(CPU::Flag::CARRY) ? 'C' : '_'
			);
		}

		if (ImGui::CollapsingHeader("Disassembly @ PC", ImGuiTreeNodeFlags_DefaultOpen)) {
			char buf[25 * 10];
			cpu.disassemble(cpu.r.pc, 10, buf);
			ImGui::Text("%s", buf);
		}
		ImGui::End();
	}


	// Audio window
	if (showDebugAudio) {
		ImGui::Begin("Audio", nullptr);
		ImGui::Checkbox("Enabled (override)", &audio.isEnabled);
		ImGui::Text("1:%s, 2:%s 3:%s, 4:%s", 
			audio.ch1.isEnabled ? "on " : "off", audio.ch2.isEnabled ? "on " : "off",
			audio.ch3.isEnabled ? "on " : "off", audio.ch4.isEnabled ? "on " : "off");

		// Show waveform of last N samples
		ImGui::PlotLines("waveform", [](void*data, int idx) { return (float)audio.sampleHistory[idx]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		ImGui::End();
	}


	// Graphics window
	if (showDebugGraphics) {
		ImGui::Begin("Graphics", nullptr);
		ImGui::Text("win: %s pos: (%02x,%02x)", (graphics.r.flags & Graphics::Flag::WINDOW) ? "on " : "off", graphics.r.winx, graphics.r.winy);

		if (ImGui::CollapsingHeader("Sprites", true)) {
			int ctr = 0;
			ImGui::Indent(GUI_INDENT_WIDTH);
			int showDetails = ImGui::CollapsingHeader("sprites on screen", false);
			for (int i = 0; i < 40; i++) {
				if ((graphics.spritedata[i].x > -8 and graphics.spritedata[i].x < 168)
					and (graphics.spritedata[i].y > -16 and graphics.spritedata[i].y < 160)) {
					++ctr;
					if (showDetails) {
						ImGui::Text("%2d (0x%2X) (%3d,%3d) f:%02X: ", i,
							graphics.spritedata[i].x, graphics.spritedata[i].y, graphics.spritedata[i].tile, graphics.spritedata[i].flags);

						ImGui::SameLine();

						int tilex = graphics.spritedata[i].tile % 16;
						int tiley = graphics.spritedata[i].tile >> 4;

						ImGui::Image((void*)((intptr_t)graphics.debugTexture), ImVec2(16,16), ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));
					}
				}
			}
			ImGui::Unindent(GUI_INDENT_WIDTH);

			if (ctr == 0) {
				ImGui::Text("(none)");
			} else {
				ImGui::Text("%d/40 sprites on screen", ctr);
			}
		}

		if (ImGui::CollapsingHeader("Background tilemap", ImGuiTreeNodeFlags_DefaultOpen)) {
			int tilemapScale = 2;

			ImVec2 tex_screen_pos = ImGui::GetCursorScreenPos();
			ImGui::Image((void*)((intptr_t)graphics.debugTexture), ImVec2(DEBUG_WIDTH * tilemapScale, DEBUG_HEIGHT * tilemapScale),
			ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				int tilex = (int)(ImGui::GetMousePos().x - tex_screen_pos.x) / (8 * tilemapScale);
				int tiley = (int)(ImGui::GetMousePos().y - tex_screen_pos.y) / (8 * tilemapScale);
				int tileaddr = 0x8000 + 0x10*(tiley*16+tilex);
				ImGui::Text("Tile: %02X @ %04X", (tileaddr & 0x0FF0) >> 4, tileaddr);
				ImGui::Image((void*)((intptr_t)graphics.debugTexture), ImVec2(128,128),
				ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));
				ImGui::EndTooltip();
			}
		}
		ImGui::End(); // debug window
	}

	// GB Screen window
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("GB Screen", nullptr, ImGuiWindowFlags_NoResize);
	ImGui::Image((void*)((intptr_t)graphics.screenTexture), ImVec2(GB_SCREEN_WIDTH * graphics.screenScale, GB_SCREEN_HEIGHT * graphics.screenScale),
		ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
	
	// handle hovering over sprites
	if (ImGui::IsItemHovered()) {
		ImVec2 tex_screen_pos = ImGui::GetCursorScreenPos();
		int mousex = (int)(ImGui::GetMousePos().x - tex_screen_pos.x);
		int mousey = (int)(ImGui::GetMousePos().y - tex_screen_pos.y);
		// check if we're on a sprite
		// for (int i = 0; i < 40; i++) {
		// 	if (spritedata[i].x > ) {
				// ImGui::BeginTooltip();
				// ImGui::Text("Sprite: %02X @ %04X", (tileaddr & 0x0FF0) >> 4, tileaddr);
				// ImGui::Image((void*)((intptr_t)debugTexture), ImVec2(80, 80),
				// ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));
				// ImGui::EndTooltip();
		// 	}
		// }
	}
	ImGui::End();
	ImGui::PopStyleVar();


	ImGui::End(); // main window
}