#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <iostream>

#include "dromaius.h"

#define GUI_INDENT_WIDTH 16.0f

// Constructor builds the window
GUI::GUI() {
	// Try to initialize SDL.
	if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
		std::cerr << "Failed to initialize SDL.\n";
		exit(1);
	}

	// Setup window
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#if __APPLE__
	// GL 3.2 Core + GLSL 150
	glsl_version = "#version 150";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
	// GL 3.0 + GLSL 130
	glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif


	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);

	// Set intiial window size
	int initialWidth, initialHeight;
	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm) != 0)
	{
		 SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
		 initialWidth = 1024;
		 initialHeight = 768;
	}
	initialWidth = dm.w * 0.75f;
	initialHeight = dm.h * 0.75f;

	
	window = SDL_CreateWindow(
		"Dromaius",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		initialWidth, initialHeight,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
	);


	if (!window) {
		printf("Failed to create a window.\n");
		exit(1);
	}

	glcontext = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, glcontext);

	// Init GL functions
	gl3wInit();

	// Setup ImGui
	initializeImgui();

	// Disable vsync, we do our own syncing
	SDL_GL_SetSwapInterval(0);

	// filtering
	//SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
}

GUI::~GUI() {
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	//SDL_GL_DeleteContext(glcontext);
	//SDL_DestroyWindow(window);
	SDL_Quit();
}

void GUI::initializeImgui() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	// Setup ImGui Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(window, glcontext);
	ImGui_ImplOpenGL3_Init(glsl_version);
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
	ImGui::Begin("Info", nullptr);

	auto romheader = (Memory::romheader_t *)emu->memory.rom; 
	ImGui::Text("ROM Name: %s\nMBC: %s\nCountry: %s\nROM size: %d\nRAM size: %d",
		romheader->gamename,
		emu->memory.mbcAsString().c_str(),
		romheader->country ? "Other" : "Japan",
		romheader->romsize,
		romheader->ramsize);
	ImGui::End(); // info window
}


void GUI::renderSettingsWindow() {
	ImGui::Begin("Controls", nullptr);
	float fps = ImGui::GetIO().Framerate;
	ImGui::Text("FPS: %.1f (%.1fx speed)", fps, fps / 61.0f);
	if (ImGui::Button("Reset ROM")) {
		emu->reset();
	}
	if (ImGui::Button("Save state")) {
		emu->saveState(0);
	}
	if (ImGui::Button("Load state")) {
		emu->loadState(0);
	}
	if (ImGui::Button("Dump emu->memory. to \nfile (memdump.bin)")) {
		emu->memory.dumpToFile("memdump.bin");
	}
	
	ImGui::Separator();

	ImGui::Checkbox("Fast forward", &emu->cpu.fastForward);
	ImGui::Checkbox("Step mode", &emu->cpu.stepMode);
	if (ImGui::Button("Step instruction (space)")) {
		emu->cpu.stepInst = true;
	}
	if (ImGui::Button("Step frame (f)")) {
		emu->cpu.stepFrame = true;
	}
	ImGui::End();
}


void GUI::renderCPUDebugWindow() {
	ImGui::Begin("CPU", nullptr);
	ImGui::Text("cycle: %d", emu->cpu.c);

	ImGui::Separator();

	ImGui::Text("interrupts: %s (enabled / flagged):", emu->cpu.intsOn ? "on " : "off");
	ImGui::Text("     VBLANK: %s / %s",
		emu->cpu.ints & CPU::Int::VBLANK ? "yes" : "no ",
		emu->cpu.intFlags & CPU::Int::VBLANK ? "yes" : "no ");
	ImGui::Text("    LCDSTAT: %s / %s",
		emu->cpu.ints & CPU::Int::LCDSTAT ? "yes" : "no ",
		emu->cpu.intFlags & CPU::Int::LCDSTAT ? "yes" : "no ");
	ImGui::Text("      TIMER: %s / %s",
		emu->cpu.ints & CPU::Int::TIMER ? "yes" : "no ",
		emu->cpu.intFlags & CPU::Int::TIMER ? "yes" : "no ");
	ImGui::Text("     SERIAL: %s / %s",
		emu->cpu.ints & CPU::Int::SERIAL ? "yes" : "no ",
		emu->cpu.intFlags & CPU::Int::SERIAL ? "yes" : "no ");
	ImGui::Text("     JOYPAD: %s / %s",
		emu->cpu.ints & CPU::Int::JOYPAD ? "yes" : "no ",
		emu->cpu.intFlags & CPU::Int::JOYPAD ? "yes" : "no ");

	ImGui::Separator();

	ImGui::Text("timer: %s", emu->cpu.timer.tac & 0x04 ? "started" : "stopped");
	ImGui::Text("    clock: %02X    div: %02X", emu->cpu.timer.tac & 0x03, emu->cpu.timer.tma);
	ImGui::Text("     tima: %02X    tma: %02X", emu->cpu.timer.tima, emu->cpu.timer.div);



	if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text(
				"a: %02X       hl: %04X\n"
				"b: %02X\n"
				"c: %02X       pc: %04X\n"
				"d: %02X       sp: %04X\n"
				"e: %02X   \n"
				"f: %c,%c,%c,%c",
			emu->cpu.r.a, (emu->cpu.r.h << 8) | emu->cpu.r.l, emu->cpu.r.b, emu->cpu.r.c,
			emu->cpu.r.pc, emu->cpu.r.d, emu->cpu.r.sp, emu->cpu.r.e,
			emu->cpu.getFlag(CPU::Flag::ZERO) ? 'Z' : '_',
			emu->cpu.getFlag(CPU::Flag::SUBTRACT) ? 'N' : '_',
			emu->cpu.getFlag(CPU::Flag::HCARRY) ? 'H' : '_',
			emu->cpu.getFlag(CPU::Flag::CARRY) ? 'C' : '_'
		);
	}


	if (ImGui::CollapsingHeader("Disassembly @ PC", ImGuiTreeNodeFlags_DefaultOpen)) {
		char buf[25 * 20];
		buf[0] = '\0';
		emu->cpu.disassemble(emu->cpu.r.pc, 20, buf);
		ImGui::Text("%s", buf);
	}

	if (ImGui::CollapsingHeader("Call stack", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("depth: %d", emu->cpu.callStackDepth);
		for (int i = 0; i < emu->cpu.callStackDepth; ++i) {
			std::string symbol = emu->memory.getSymbolName(emu->memory.romBank, emu->cpu.callStack[i]);
			if (symbol != "") {
				ImGui::Text("%d: %04X (%s)", i, emu->cpu.callStack[i], symbol.c_str());
			} else {
				ImGui::Text("%d: %04X", i, emu->cpu.callStack[i]);
			}
		}
	}

	ImGui::End();
}

// hack for lambda..
Dromaius *g_emu;

void GUI::renderAudioWindow() {
	ImGui::Begin("Audio", nullptr);
	ImGui::Text("Enabled: %s", emu->audio.isEnabled ? "yes" : "no");

	ImGui::Separator();

	// Show waveram values as plot
	g_emu = emu;
	ImGui::Text("waveram: ");
	ImGui::SameLine();
	ImGui::PlotLines("",
		[](void *data, int idx) { return (float)(g_emu->audio.waveRam[idx/2] & ((idx % 2) ? 0xF0 : 0x0F));}, NULL, 32);

	ImGui::Separator();

	// Show waveforms of last N samples
	ImGui::Text("ch1 (%s): ", emu->audio.ch1.isEnabled ? "on " : "off");
	ImGui::SameLine();
	ImGui::PlotLines("", [](void*data, int idx) { return (float)g_emu->audio.sampleHistory[0][(idx + g_emu->cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
	
	ImGui::Text("ch2 (%s): ", emu->audio.ch2.isEnabled ? "on " : "off");
	ImGui::SameLine();
	ImGui::PlotLines("", [](void*data, int idx) { return (float)g_emu->audio.sampleHistory[1][(idx + g_emu->cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
	
	ImGui::Text("ch3 (%s): ", emu->audio.ch3.isEnabled ? "on " : "off");
	ImGui::SameLine();
	ImGui::PlotLines("", [](void*data, int idx) { return (float)g_emu->audio.sampleHistory[2][(idx + g_emu->cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
	
	ImGui::Text("ch4 (%s): ", emu->audio.ch4.isEnabled ? "on " : "off");
	ImGui::SameLine();
	ImGui::PlotLines("", [](void*data, int idx) { return (float)g_emu->audio.sampleHistory[3][(idx + g_emu->cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
	ImGui::End();
}


void GUI::renderGraphicsDebugWindow() {
	ImGui::Begin("Graphics", nullptr);

	// Basic flags info dump
	ImGui::Text("background: %s", (emu->graphics.r.flags & Graphics::Flag::BG) ? "on " : "off");
	ImGui::Text("    tileset: %s", (emu->graphics.r.flags & Graphics::Flag::TILESET) ? "8000-8FFF" : "8800-97FF");
	ImGui::Text("    tilemap: %s", (emu->graphics.r.flags & Graphics::Flag::TILEMAP) ? "9C00-9FFF" : "9800-9BFF");
	ImGui::Text("    scroll: (%02X,%02X)", emu->graphics.r.scx, emu->graphics.r.scy);
	ImGui::Separator();
	ImGui::Text("window: %s", (emu->graphics.r.flags & Graphics::Flag::WINDOW) ? "on " : "off");
	ImGui::Text("    position: (%02X,%02X)", emu->graphics.r.winx, emu->graphics.r.winy);
	ImGui::Text("    tilemap: %s", (emu->graphics.r.flags & Graphics::Flag::WINDOWTILEMAP) ? "9C00-9FFF" : "9800-9BFF");
	ImGui::Separator();
	ImGui::Text("sprites: %s", (emu->graphics.r.flags & Graphics::Flag::SPRITES) ? "on " : "off");
	ImGui::Text("    size: %s", (emu->graphics.r.flags & Graphics::Flag::SPRITESIZE) ? "8x16" : "8x8");
	ImGui::Separator();
	ImGui::Text("GPU state:");
	ImGui::Text("    mode: %s", emu->graphics.modeToString(emu->graphics.mode));
	ImGui::Text("    line: %03d/%d (comp: %d)", emu->graphics.r.line, 144, emu->graphics.r.lineComp);



	if (ImGui::CollapsingHeader("Sprites", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Columns(5);
		// hack to make columns non resizable
		ImGui::GetCurrentWindow()->DC.CurrentColumns[0].Flags |= ImGuiColumnsFlags_NoResize;
		ImGui::GetCurrentWindow()->DC.CurrentColumns[1].Flags |= ImGuiColumnsFlags_NoResize;
		ImGui::GetCurrentWindow()->DC.CurrentColumns[2].Flags |= ImGuiColumnsFlags_NoResize;
		ImGui::GetCurrentWindow()->DC.CurrentColumns[3].Flags |= ImGuiColumnsFlags_NoResize;
		ImGui::GetCurrentWindow()->DC.CurrentColumns[4].Flags |= ImGuiColumnsFlags_NoResize;

		for (int i = 0; i < 40; i++) {
			int spriteVisible = ((emu->graphics.spritedata[i].x > -8 and emu->graphics.spritedata[i].x < 168)
				and (emu->graphics.spritedata[i].y > -16 and emu->graphics.spritedata[i].y < 160));

			// slightly make elements transparent if not visible
			if (not spriteVisible) {
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
			}

			ImGui::Text("%2d:", i);
			ImGui::SameLine();

			if (spriteVisible) {
				int tilex = emu->graphics.spritedata[i].tile % 16;
				int tiley = emu->graphics.spritedata[i].tile >> 4;

				// Draw image with details on hover
				ImGui::Image((void*)((intptr_t)emu->graphics.debugTexture), ImVec2(16,16), ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));
				renderHoverText("%2d (0x%2X) (%3d,%3d) f:%02X: ", i,
					emu->graphics.spritedata[i].x, emu->graphics.spritedata[i].y,
					emu->graphics.spritedata[i].tile, emu->graphics.spritedata[i].flags);

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
		ImGui::Image((void*)((intptr_t)emu->graphics.debugTexture), ImVec2(DEBUG_WIDTH * tilemapScale, DEBUG_HEIGHT * tilemapScale),
		ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			int tilex = (int)(ImGui::GetMousePos().x - tex_screen_pos.x) / (8 * tilemapScale);
			int tiley = (int)(ImGui::GetMousePos().y - tex_screen_pos.y) / (8 * tilemapScale);
			int tileaddr = 0x8000 + 0x10*(tiley*16+tilex);
			ImGui::Text("Tile: %03X @ %04X", (tileaddr & 0x1FF0) >> 4, tileaddr);
			ImGui::Image((void*)((intptr_t)emu->graphics.debugTexture), ImVec2(128,128),
			ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));
			ImGui::EndTooltip();
		}

		// Grey out the inactive tiles
		int topInactiveTile, bottomInactiveLine;
		if (emu->graphics.r.flags & Graphics::Flag::TILESET) {
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

void GUI::renderMemoryViewerWindow() {
	uint16_t lineBuffer[16];
	uint8_t charBuffer[16];

	ImGui::Begin("Memory viewer", nullptr);
 	
	ImGui::Columns(4);

	// hack to make columns non resizable
	ImGui::GetCurrentWindow()->DC.CurrentColumns[0].Flags |= ImGuiColumnsFlags_NoResize;
	ImGui::GetCurrentWindow()->DC.CurrentColumns[1].Flags |= ImGuiColumnsFlags_NoResize;
	ImGui::GetCurrentWindow()->DC.CurrentColumns[2].Flags |= ImGuiColumnsFlags_NoResize;
	ImGui::GetCurrentWindow()->DC.CurrentColumns[3].Flags |= ImGuiColumnsFlags_NoResize;

	ImGui::SetColumnWidth(0, 60);
	ImGui::SetColumnWidth(1, 50);
	ImGui::SetColumnWidth(2, 300);
	ImGui::SetColumnWidth(3, 150);

	uint16_t startAddr = 0x000;
	uint16_t endAddr   = 0xFFF;

	// Clip invisible lines
	ImGuiListClipper clipper(endAddr - startAddr, ImGui::GetTextLineHeight());
	for (int addr = clipper.DisplayStart; addr < clipper.DisplayEnd; ++addr) {
		// Region
		ImGui::Text("%s", emu->memory.getRegionName(addr << 4).c_str());
		ImGui::NextColumn();

		// Address
		ImGui::Text("%03X0", addr);
		ImGui::NextColumn();

		for (int i = 0; i < 16; ++i) {
			lineBuffer[i] = emu->memory.readByte((addr << 4) + i);
			charBuffer[i] = (lineBuffer[i] >= 0x20 and lineBuffer[i] <= 0x7E) ? lineBuffer[i] : '.';
		}

		ImGui::Text("%02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X",
			lineBuffer[0], lineBuffer[1], lineBuffer[2], lineBuffer[3], 
			lineBuffer[4], lineBuffer[5], lineBuffer[6], lineBuffer[7], 
			lineBuffer[8], lineBuffer[9], lineBuffer[10], lineBuffer[11], 
			lineBuffer[12], lineBuffer[13], lineBuffer[14], lineBuffer[15]
		);

		ImGui::NextColumn();

		ImGui::Text("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
			charBuffer[0], charBuffer[1], charBuffer[2], charBuffer[3], 
			charBuffer[4], charBuffer[5], charBuffer[6], charBuffer[7], 
			charBuffer[8], charBuffer[9], charBuffer[10], charBuffer[11], 
			charBuffer[12], charBuffer[13], charBuffer[14], charBuffer[15]
		);

		ImGui::NextColumn();

	}
	clipper.End();

	ImGui::End();
}


void GUI::renderGBScreenWindow() {
	// GB Screen window
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("GB Screen", nullptr, ImGuiWindowFlags_NoResize);
	ImGui::Image((void*)((intptr_t)emu->graphics.screenTexture), ImVec2(GB_SCREEN_WIDTH * emu->graphics.screenScale, GB_SCREEN_HEIGHT * emu->graphics.screenScale),
		ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
	ImGui::End();
	ImGui::PopStyleVar();
}


void GUI::render() {
	// Start a new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();

	// Set global style
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;

	// Scale main window up to SDL window size
	int windowWidth, windowHeight;
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	ImGui::SetNextWindowPos(ImVec2(.0f, .0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::Begin("Main window", nullptr, 
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
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
			ImGui::MenuItem("CPU info", nullptr, &showCPUDebugWindow);
			ImGui::MenuItem("Graphics info", nullptr, &showGraphicsDebugWindow);
			ImGui::MenuItem("Audio info", nullptr, &showAudioWindow);
			ImGui::MenuItem("emu->memory. viewer", nullptr, &showMemoryViewerWindow);
			
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	renderInfoWindow();
	renderSettingsWindow();
	renderGBScreenWindow();

	if (showCPUDebugWindow)
		renderCPUDebugWindow();

	if (showAudioWindow)
		renderAudioWindow();

	if (showGraphicsDebugWindow)
		renderGraphicsDebugWindow();

	if (showMemoryViewerWindow)
		renderMemoryViewerWindow();


	ImGui::End(); // main window

	ImGui::Render();
}