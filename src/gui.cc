#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdarg>

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

void GUI::renderHoverText(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);


	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::TextV(fmt, args);
		ImGui::EndTooltip();
	}

	va_end(args);
}

void GUI::renderInfoWindow() {
	ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoResize);
	ImGui::Text("ROM Name: %s\nMBC: %s\nCountry: %s\nROM size: %d\nRAM size: %d",
		memory.romheader->gamename,
		memory.mbcAsString().c_str(),
		memory.romheader->country ? "Other" : "Japan",
		memory.romheader->romsize,
		memory.romheader->ramsize);
	ImGui::End(); // info window
}


void GUI::renderSettingsWindow() {
	ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoResize);
	float fps = ImGui::GetIO().Framerate;
	ImGui::Text("FPS: %.1f (%.1fx speed)", fps, fps / 61.0f);
	if (ImGui::Button("Reset ROM")) {
		resetEmulation();
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
}


void GUI::renderCPUDebugWindow() {
	if (showDebugCPU) {
		ImGui::Begin("CPU", nullptr);
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
			char buf[25 * 20];
			buf[0] = '\0';
			cpu.disassemble(cpu.r.pc, 20, buf);
			ImGui::Text("%s", buf);
		}
		ImGui::End();
	}
}


void GUI::renderAudioWindow() {
	if (showDebugAudio) {
		ImGui::Begin("Audio", nullptr);
		ImGui::Text("Enabled: %s", audio.isEnabled ? "yes" : "no");

		ImGui::Separator();

		// Show waveram values as plot
		ImGui::Text("waveram: ");
		ImGui::SameLine();
		ImGui::PlotLines("",
			[](void*data, int idx) { return (float)(audio.waveRam[idx/2] & ((idx % 2) ? 0xF0 : 0x0F));}, NULL, 32);

		ImGui::Separator();

		// Show waveforms of last N samples
		ImGui::Text("ch1 (%s): ", audio.ch1.isEnabled ? "on " : "off");
		ImGui::SameLine();
		ImGui::PlotLines("", [](void*data, int idx) { return (float)audio.sampleHistory[0][(idx + cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		
		ImGui::Text("ch2 (%s): ", audio.ch2.isEnabled ? "on " : "off");
		ImGui::SameLine();
		ImGui::PlotLines("", [](void*data, int idx) { return (float)audio.sampleHistory[1][(idx + cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		
		ImGui::Text("ch3 (%s): ", audio.ch3.isEnabled ? "on " : "off");
		ImGui::SameLine();
		ImGui::PlotLines("", [](void*data, int idx) { return (float)audio.sampleHistory[2][(idx + cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		
		ImGui::Text("ch4 (%s): ", audio.ch4.isEnabled ? "on " : "off");
		ImGui::SameLine();
		ImGui::PlotLines("", [](void*data, int idx) { return (float)audio.sampleHistory[3][(idx + cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		ImGui::End();
	}
}


void GUI::renderGraphicsDebugWindow() {
	if (showDebugGraphics) {
		ImGui::Begin("Graphics", nullptr);

		// Basic flags info dump
		ImGui::Text("background: %s", (graphics.r.flags & Graphics::Flag::BG) ? "on " : "off");
		ImGui::Text("    tileset: %s", (graphics.r.flags & Graphics::Flag::TILESET) ? "8000-8FFF" : "8800-97FF");
		ImGui::Text("    tilemap: %s", (graphics.r.flags & Graphics::Flag::TILEMAP) ? "9C00-9FFF" : "9800-9BFF");
		ImGui::Text("    scroll: (%02X,%02X)", graphics.r.scx, graphics.r.scy);
		ImGui::Separator();
		ImGui::Text("window: %s", (graphics.r.flags & Graphics::Flag::WINDOW) ? "on " : "off");
		ImGui::Text("    position: (%02X,%02X)", graphics.r.winx, graphics.r.winy);
		ImGui::Text("    tilemap: %s", (graphics.r.flags & Graphics::Flag::WINDOWTILEMAP) ? "9C00-9FFF" : "9800-9BFF");
		ImGui::Separator();
		ImGui::Text("sprites: %s", (graphics.r.flags & Graphics::Flag::SPRITES) ? "on " : "off");
		ImGui::Text("    size: %s", (graphics.r.flags & Graphics::Flag::SPRITESIZE) ? "8x16" : "8x8");
		ImGui::Separator();
		ImGui::Text("GPU state:");
		ImGui::Text("    mode: %s", graphics.modeToString(graphics.mode));
		ImGui::Text("    line: %03d/%d (comp: %d)", graphics.r.line, 144, graphics.r.lineComp);



		if (ImGui::CollapsingHeader("Sprites", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(5);
			for (int i = 0; i < 40; i++) {
				int spriteVisible = ((graphics.spritedata[i].x > -8 and graphics.spritedata[i].x < 168)
					and (graphics.spritedata[i].y > -16 and graphics.spritedata[i].y < 160));

				// slightly make elements transparent if not visible
				if (not spriteVisible) {
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
				}

				ImGui::Text("%2d:", i);
				ImGui::SameLine();

				if (spriteVisible) {
					int tilex = graphics.spritedata[i].tile % 16;
					int tiley = graphics.spritedata[i].tile >> 4;

					// Draw image with details on hover
					ImGui::Image((void*)((intptr_t)graphics.debugTexture), ImVec2(16,16), ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));
					renderHoverText("%2d (0x%2X) (%3d,%3d) f:%02X: ", i,
						graphics.spritedata[i].x, graphics.spritedata[i].y,
						graphics.spritedata[i].tile, graphics.spritedata[i].flags);

				} else {
					// Sprite not on screen
					// Show spacer to make the UI not flicker when showing/hiding sprites
					ImGui::Dummy(ImVec2(16.0f, 16.0f));
				}

				if (not spriteVisible) {
					ImGui::PopStyleVar();
				}

				ImGui::NextColumn();
				if (i % 5 == 0)
					ImGui::Separator();
			}
			ImGui::Columns(1);
		}

		if (ImGui::CollapsingHeader("Background tileset", ImGuiTreeNodeFlags_DefaultOpen)) {
			int tilemapScale = 2;

			ImVec2 tex_screen_pos = ImGui::GetCursorScreenPos();
			ImGui::Image((void*)((intptr_t)graphics.debugTexture), ImVec2(DEBUG_WIDTH * tilemapScale, DEBUG_HEIGHT * tilemapScale),
			ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				int tilex = (int)(ImGui::GetMousePos().x - tex_screen_pos.x) / (8 * tilemapScale);
				int tiley = (int)(ImGui::GetMousePos().y - tex_screen_pos.y) / (8 * tilemapScale);
				int tileaddr = 0x8000 + 0x10*(tiley*16+tilex);
				ImGui::Text("Tile: %03X @ %04X", (tileaddr & 0x1FF0) >> 4, tileaddr);
				ImGui::Image((void*)((intptr_t)graphics.debugTexture), ImVec2(128,128),
				ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));
				ImGui::EndTooltip();
			}

			// Grey out the inactive tiles
			int topInactiveTile, bottomInactiveLine;
			if (graphics.r.flags & Graphics::Flag::TILESET) {
				// active: 8000-8FFF
				topInactiveTile = 16 * 8;
				bottomInactiveLine = 24 * 8;
			} else {
				// active: 8800-97FF
				topInactiveTile = 0;
				bottomInactiveLine = 8 * 8;
			}
			ImGui::GetWindowDrawList()->AddRectFilled(
				ImVec2(tex_screen_pos.x, tex_screen_pos.y + topInactiveTile * tilemapScale),
				ImVec2(tex_screen_pos.x + DEBUG_WIDTH * tilemapScale, tex_screen_pos.y + bottomInactiveLine * tilemapScale),
				IM_COL32(0,0,0,128)
			);
		}
		ImGui::End(); // debug window
	}
}


void GUI::renderGBScreenWindow() {
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
		// TODO: check if we're on a sprite and show popup
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

	renderInfoWindow();
	renderSettingsWindow();
	renderCPUDebugWindow();
	renderAudioWindow();
	renderGraphicsDebugWindow();
	renderGBScreenWindow();

	ImGui::End(); // main window
}