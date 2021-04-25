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

	// Set intial window size
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

	if (emu->memory.romLoaded) {
		auto romheader = (Memory::romheader_t *)(emu->memory.rom + 0x134); 
		ImGui::Text("ROM: %s", emu->filename.c_str());
		ImGui::Text("ROM Name: %s\nMBC: %s\nCountry: %s\nType: %s\nROM size: %s\nRAM size: %s",
			romheader->gamename,
			emu->memory.mbcAsString().c_str(),
			romheader->country ? "Other" : "Japan",
			emu->memory.getCartridgeTypeString(romheader->type).c_str(),
			emu->memory.getCartridgeRomSizeString(romheader->romsize).c_str(),
			emu->memory.getCartridgeRomSizeString(romheader->ramsize).c_str()
		);
	} else {
		ImGui::Text("No ROM loaded");
		if (ImGui::Button("Load ROM...")) {
			triggerRomLoadDialog();
		}
	}
	ImGui::End(); // info window
}


void GUI::renderSettingsWindow() {
	ImGui::Begin("Controls", nullptr);

	if (emu->memory.romLoaded) {
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
		if (ImGui::Button("Dump memory to \nfile (memdump.bin)")) {
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
	}
	ImGui::End();
}


void GUI::renderCPUDebugWindow() {
	ImGui::Begin("CPU", nullptr);

	if (emu->memory.romLoaded) {
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
		ImGui::Text("      tac: %02X    tma: %02X", emu->cpu.timer.tac, emu->cpu.timer.tma);
		ImGui::Text("     tima: %02X    div: %02X", emu->cpu.timer.tima, emu->cpu.timer.div);



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
				std::string symbol = emu->memory.getSymbolFromAddress(emu->memory.romBank, emu->cpu.callStack[i]);
				if (symbol != "") {
					ImGui::Text("%d: %04X (%s)", i, emu->cpu.callStack[i], symbol.c_str());
				} else {
					ImGui::Text("%d: %04X", i, emu->cpu.callStack[i]);
				}
			}
		}
	}

	ImGui::End();
}

// hack for lambda..
Dromaius *g_emu;

void GUI::renderAudioWindow() {
	ImGui::Begin("Audio", nullptr);

	if (emu->memory.romLoaded) {
		ImGui::Text("Enabled: %s", emu->audio.isEnabled ? "yes" : "no");

		ImGui::Separator();

		// Show waveram values as plot
		g_emu = emu;
		ImGui::Text("waveram: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::PlotLines("",
			[](void *data, int idx) { return (float)(g_emu->audio.waveRam[idx/2] & ((idx % 2) ? 0xF0 : 0x0F));}, NULL, 32);
		ImGui::PopItemWidth();

		ImGui::Separator();

		// Show waveforms of last N samples
		ImGui::Text("ch1 (%s): ", emu->audio.ch1.isEnabled ? "on " : "off");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::PlotLines("", [](void*data, int idx) { return (float)g_emu->audio.sampleHistory[0][(idx + g_emu->cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		ImGui::PopItemWidth();

		ImGui::Text("ch2 (%s): ", emu->audio.ch2.isEnabled ? "on " : "off");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::PlotLines("", [](void*data, int idx) { return (float)g_emu->audio.sampleHistory[1][(idx + g_emu->cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		ImGui::PopItemWidth();
		
		ImGui::Text("ch3 (%s): ", emu->audio.ch3.isEnabled ? "on " : "off");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::PlotLines("", [](void*data, int idx) { return (float)g_emu->audio.sampleHistory[2][(idx + g_emu->cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		ImGui::PopItemWidth();
		
		ImGui::Text("ch4 (%s): ", emu->audio.ch4.isEnabled ? "on " : "off");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::PlotLines("", [](void*data, int idx) { return (float)g_emu->audio.sampleHistory[3][(idx + g_emu->cpu.c) % AUDIO_SAMPLE_HISTORY_SIZE]; }, NULL, AUDIO_SAMPLE_HISTORY_SIZE);
		ImGui::PopItemWidth();
	}

	ImGui::End();
}


void GUI::renderGraphicsDebugWindow() {
	ImGui::Begin("Graphics", nullptr);

	if (emu->memory.romLoaded) {
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
			if (ImGui::BeginTable("sprites", 5, ImGuiTableFlags_SizingFixedFit)) {

				// Column sizing
				ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_None, 16);
				ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_None, 16);
				ImGui::TableSetupColumn("3", ImGuiTableColumnFlags_None, 16);
				ImGui::TableSetupColumn("4", ImGuiTableColumnFlags_None, 16);
				ImGui::TableSetupColumn("5", ImGuiTableColumnFlags_None, 16);

				for (int i = 0; i < 40; i++) {
					int spriteVisible = ((emu->graphics.spritedata[i].x > -8 and emu->graphics.spritedata[i].x < 168)
						and (emu->graphics.spritedata[i].y > -16 and emu->graphics.spritedata[i].y < 160));

					// slightly make elements transparent if not visible
					if (not spriteVisible) {
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
					}

					if (spriteVisible) {
						int tilex = emu->graphics.spritedata[i].tile % 16;
						int tiley = emu->graphics.spritedata[i].tile >> 4;

						// Draw image with details on hover
						ImGui::Image((void*)((intptr_t)emu->graphics.debugTexture), ImVec2(16,16), ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));

					} else {
						// Sprite not on screen
						// Show spacer to make the UI not flicker when showing/hiding sprites
						ImGui::Dummy(ImVec2(16.0f, 16.0f));
					}
					renderHoverText("%2d (0x%2X) (%3d,%3d) f:%02X: ", i,
						emu->graphics.spritedata[i].x, emu->graphics.spritedata[i].y,
						emu->graphics.spritedata[i].tile, emu->graphics.spritedata[i].flags);

					if (not spriteVisible) {
						ImGui::PopStyleVar();
					}

					ImGui::TableNextColumn();
				}

				ImGui::EndTable();
			}
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
	}
	ImGui::End(); // debug window
}

void GUI::renderMemoryViewerWindow() {
	uint16_t lineBuffer[16];
	uint8_t charBuffer[16];
	int jumpAddr = -1;

	const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

	ImGui::Begin("Memory viewer", nullptr);

	if (emu->memory.romLoaded) {
		if (ImGui::BeginTable("jumps", 2, ImGuiTableFlags_BordersInnerV)) {
			// Jump to addr
			static char hexBuf[5] = {0x00};
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(100);
			ImGui::InputTextWithHint("###jump to", "address", hexBuf, 5,
				ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
			ImGui::SameLine();
			if (ImGui::Button("<- jump to addr")) {
				jumpAddr = (int)strtol(hexBuf, NULL, 16);
			}

			// Jump to SP
			ImGui::TableNextColumn();
			ImGui::Text("Jump to: ");
			ImGui::SameLine();
			if (ImGui::Button("SP")) {
				jumpAddr = emu->cpu.r.sp;
			}
			ImGui::SameLine();
			if (ImGui::Button("PC")) {
				jumpAddr = emu->cpu.r.pc;
			}

			ImGui::EndTable();
		}

	 	
	 	ImGuiTableFlags flags = 
	 		ImGuiTableFlags_BordersV | 
	 		ImGuiTableFlags_RowBg |
	 		ImGuiTableFlags_SizingFixedFit;

	 	if (ImGui::BeginTable("headers", 4, flags)) {
		 	ImGui::TableSetupColumn("Region", ImGuiTableColumnFlags_None, 60);
		 	ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_None, 50);
		 	ImGui::TableSetupColumn("0 1  2 3  4 5  6 7  8 9  A B  C D  E F", ImGuiTableColumnFlags_None, 300);
		 	ImGui::TableSetupColumn("0123456789ABCDEF", ImGuiTableColumnFlags_None, 150);
		 	ImGui::TableHeadersRow();
		 	ImGui::EndTable();
	 	}

	 	ImVec2 child_size = ImVec2(0, 0);
	 	ImGui::BeginChild("##ScrollingRegion", child_size);//, false);

	 	// Jump to addr if clicked
	 	if (jumpAddr != -1) {
	 		ImGui::SetScrollY((jumpAddr >> 4) * TEXT_BASE_HEIGHT);
	 	}

		if(ImGui::BeginTable("mem", 4, flags)) {

			// ImGui::TableSetColumnWidth(0, 60);
			// ImGui::TableSetColumnWidth(1, 50);
			// ImGui::TableSetColumnWidth(2, 300);
			// ImGui::TableSetColumnWidth(3, 150);
			ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_None, 60);
			ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_None, 50);
			ImGui::TableSetupColumn("3", ImGuiTableColumnFlags_None, 300);
			ImGui::TableSetupColumn("4", ImGuiTableColumnFlags_None, 150);

			uint16_t startAddr = 0x000;
			uint16_t endAddr   = 0xFFF;

			ImGuiListClipper clipper;//(endAddr - startAddr, ImGui::GetTextLineHeight());
			clipper.Begin(endAddr - startAddr);
			while(clipper.Step()) {
				for (int addr = clipper.DisplayStart; addr < clipper.DisplayEnd; ++addr) {
					ImGui::TableNextRow();

					// Region
					ImGui::TableNextColumn();
					ImGui::Text("%s", emu->memory.getRegionName(addr << 4).c_str());

					// Address
					ImGui::TableNextColumn();
					ImGui::Text("%03X0", addr);

					for (int i = 0; i < 16; ++i) {
						lineBuffer[i] = emu->memory.readByte((addr << 4) + i);
						charBuffer[i] = (lineBuffer[i] >= 0x20 and lineBuffer[i] <= 0x7E) ? lineBuffer[i] : '.';
					}

					// Hex bytes
					ImGui::TableNextColumn();
					auto hex_screen_pos = ImGui::GetCursorScreenPos();
					ImGui::Text("%02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X",
						lineBuffer[0], lineBuffer[1], lineBuffer[2], lineBuffer[3], 
						lineBuffer[4], lineBuffer[5], lineBuffer[6], lineBuffer[7], 
						lineBuffer[8], lineBuffer[9], lineBuffer[10], lineBuffer[11], 
						lineBuffer[12], lineBuffer[13], lineBuffer[14], lineBuffer[15]
					);
					if (ImGui::IsItemHovered()) {
						int offset = (int)(ImGui::GetMousePos().x - hex_screen_pos.x) / TEXT_BASE_WIDTH;
						
						// Don't show popup on space
						if (offset % 5 != 4) {
							auto hoverAddr = (addr << 8) + (offset / 5) * 2 + (offset % 5) / 2;

							ImGui::BeginTooltip();
							ImGui::Text("0x%04X", hoverAddr);
							ImGui::EndTooltip();
						}
					}

					// ASCII
					ImGui::TableNextColumn();
					hex_screen_pos = ImGui::GetCursorScreenPos();
					ImGui::Text("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
						charBuffer[0], charBuffer[1], charBuffer[2], charBuffer[3], 
						charBuffer[4], charBuffer[5], charBuffer[6], charBuffer[7], 
						charBuffer[8], charBuffer[9], charBuffer[10], charBuffer[11], 
						charBuffer[12], charBuffer[13], charBuffer[14], charBuffer[15]
					);
					if (ImGui::IsItemHovered()) {
						int offset = (int)(ImGui::GetMousePos().x - hex_screen_pos.x) / TEXT_BASE_WIDTH;
						auto hoverAddr = (addr << 8) + offset;

						ImGui::BeginTooltip();
						ImGui::Text("0x%04X", hoverAddr);
						ImGui::EndTooltip();
					}

				}
			}

			ImGui::EndTable();
		}
		ImGui::EndChild();
	}
	ImGui::End();
}


void GUI::renderGameSpecificWindow() {
	ImGui::Begin("Game-specific", nullptr);

	if (emu->memory.romLoaded) {
		// Window contents is game-dependent
		auto romheader = (Memory::romheader_t *)(emu->memory.rom + 0x134); 
		auto gameName = std::string(romheader->gamename);
		if (gameName == "POKEMON RED") {
			gameGUI_pokemon_red(emu);
		} else {
			ImGui::Text("Sorry, no special features for this game.");
		}
	}

	ImGui::End();
}



void GUI::renderGBScreenWindow() {
	// GB Screen window
	// ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("LCD", nullptr);

	if (emu->memory.romLoaded) {
		// Scaling	
		ImGui::SliderInt("Scale factor", (int *)&emu->graphics.screenScale, 1, 5);

		// Center the image
		auto image_size = ImVec2(GB_SCREEN_WIDTH * emu->graphics.screenScale, GB_SCREEN_HEIGHT * emu->graphics.screenScale);
		auto window_size = ImGui::GetWindowSize();
		ImGui::SetCursorPos(ImVec2((int)(window_size.x - image_size.x)/2, (int)(window_size.y - image_size.y)/2));

		// Draw image
		ImGui::Image((void*)((intptr_t)emu->graphics.screenTexture), image_size,
			ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
	}

	ImGui::End();
}

void GUI::renderConsoleWindow() {
	// Console window
	ImGui::Begin("Console", nullptr);

	// TODO: circular log buffer

	ImGui::Text("TODO");
	
	ImGui::End();
}


void GUI::triggerRomLoadDialog() {
	openRomDialog.SetTitle("Select a GB ROM");
	openRomDialog.SetTypeFilters({".gb"});
	openRomDialog.Open();
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


	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowPos(ImVec2(.0f, .0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::Begin("Main window", nullptr, 
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_MenuBar
	);
	ImGui::PopStyleVar();

	// Menubar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open...")) {
				triggerRomLoadDialog();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) {
				printf("TODO exit\n");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Windows"))
		{
			ImGui::MenuItem("CPU info", nullptr, &showCPUDebugWindow);
			ImGui::MenuItem("Graphics info", nullptr, &showGraphicsDebugWindow);
			ImGui::MenuItem("Audio info", nullptr, &showAudioWindow);
			ImGui::MenuItem("Memory viewer", nullptr, &showMemoryViewerWindow);
			ImGui::MenuItem("imgui demo window", nullptr, &showImguiDemoWindow);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	// === Docking code ===
	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	// === End docking code ===

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

	if (showGameSpecificWindow)
		renderGameSpecificWindow();

	if (showImguiDemoWindow)
		ImGui::ShowDemoWindow();


	ImGui::End(); // main window

	// Display open ROM dialog if it is open
	openRomDialog.Display();

	if(openRomDialog.HasSelected())
	{
		auto filename = openRomDialog.GetSelected().string();
		openRomDialog.ClearSelected();

		emu->unloadRom();
		emu->initializeWithRom(filename.c_str());
	}

	ImGui::Render();
}