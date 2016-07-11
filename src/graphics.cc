#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "gbemu.h"

#include "audio.h"
#include "cpu.h"
#include "graphics.h"
#include "input.h"
#include "memory.h"

extern CPU cpu;
extern Audio audio;
extern Input input;
extern Memory memory;


Graphics::~Graphics()
{
	if (initialized) {
		freeBuffers();
	}
}

void Graphics::initialize()
{
	mode = Mode::HBLANK;
	mclock = 0;
	r.line = 0;
	r.scx = 0;
	r.scy = 0;
	r.flags = 0;

	screenPixels = new uint32_t[GB_SCREEN_WIDTH * GB_SCREEN_HEIGHT];
	debugPixels = new uint32_t[DEBUG_WIDTH * DEBUG_HEIGHT];

	screenScale = 2;

	vram = new uint8_t[0x2000];
	oam = new uint8_t[0xA0];
	
	tileset = new uint8_t **[0x200]; // = 512 dec
	for (int i = 0; i < 512; i++) {
		tileset[i] = new uint8_t*[8];
		for (int j = 0; j < 8; j++) {
			tileset[i][j] = new uint8_t[8];
		}
	}
	
	spritedata = new sprite_s[0x28]; // = 40 dec
	for (int i = 0; i < 40; i++) {
		spritedata[i].x = -8;
		spritedata[i].y = -16;
		spritedata[i].tile = 0;
		spritedata[i].flags = 0;
	}

	initialized = true;
}

void Graphics::freeBuffers()
{
	delete[] screenPixels;
	delete[] debugPixels;

	delete[] vram;
	delete[] oam;
	
	for (int i = 0; i < 512; i++) {
		for (int j = 0; j < 8; j++) {
			delete[] tileset[i][j];
		}
		delete[] tileset[i];
	}
	delete[] tileset;
	
	delete[] spritedata;
}

void Graphics::initDisplay()
{

	// Setup window
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	
	mainWindow = SDL_CreateWindow(memory.romheader->gamename,
	                      SDL_WINDOWPOS_CENTERED,
	                      SDL_WINDOWPOS_CENTERED,
	                      WINDOW_WIDTH, WINDOW_HEIGHT,
	                      SDL_WINDOW_OPENGL);

	if (!mainWindow) {
		printf("Failed to create a window.\n");
		exit(1);
	}


    SDL_GLContext glcontext = SDL_GL_CreateContext(mainWindow);
    printf("context = %u\n", glcontext);

    // Init GL functions
    gl3wInit();

    // Setup ImGui binding
    ImGui_ImplSdlGL3_Init(mainWindow);

    // Create texture for game graphics
    glGenTextures(1, &screenTexture);

    // Create texture for video mem debug
    glGenTextures(1, &debugTexture);


//	screenRenderer = SDL_CreateRenderer(mainWindow, -1, 0);
//	debugRenderer = SDL_CreateRenderer(debugWindow, -1, 0);
//
//	SDL_RenderSetLogicalSize(screenRenderer, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
//	SDL_RenderSetLogicalSize(debugRenderer, DEBUG_WIDTH, DEBUG_HEIGHT);
//
//	screenTexture = SDL_CreateTexture(screenRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT);
//	debugTexture = SDL_CreateTexture(debugRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, DEBUG_WIDTH, DEBUG_HEIGHT);

	// filtering
	//SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
}

void Graphics::updateTextures()
{
	// Bind texture and upload pixels
	glBindTexture(GL_TEXTURE_2D, screenTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GB_SCREEN_WIDTH, GB_SCREEN_HEIGHT,
		0, GL_RGBA, GL_UNSIGNED_BYTE, screenPixels);

	// Bind texture and upload pixels
	glBindTexture(GL_TEXTURE_2D, debugTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DEBUG_WIDTH, DEBUG_HEIGHT,
		0, GL_RGBA, GL_UNSIGNED_BYTE, debugPixels);

	// Restore state
	glBindTexture(GL_TEXTURE_2D, 0);
}

uint8_t Graphics::readByte(uint16_t addr)
{
	uint8_t res;
	
	switch (addr) {
		case 0:
			return r.flags;

		case 1: // LCD status
			//printf("  r.line = %d, r.lineComp = %d.\n", r.line, r.lineComp);
			return (mode & 0x03) +
				(r.line == r.lineComp ? 0x04 : 0x00) +
				(hBlankInt ? 0x08 : 0x00) +
				(vBlankInt ? 0x10 : 0x00) +
				(OAMInt ? 0x20 : 0x00) +
				(CoinInt ? 0x40 : 0x00);

		case 2:
			return r.scy;
			
		case 3:
			return r.scx;
			
		case 4:
			return r.line;

		case 5:
			return r.lineComp;
			
		default:
			//printf("TODO! read from unimplemented 0x%02X\n", addr + 0xFF40);
			return 0x00; // TODO: Unhandled I/O, no idea what GB does here
	}
}

void Graphics::writeByte(uint8_t b, uint16_t addr)
{
	switch (addr) {
		case 0:
			r.flags = b;
			break;

		case 1: // LCD status, writing is only allowed on the interrupt enable flags
			hBlankInt = (b & 0x08 ? 1 : 0);
			vBlankInt = (b & 0x10 ? 1 : 0);	
			OAMInt = (b & 0x20 ? 1 : 0);	
			CoinInt = (b & 0x40 ? 1 : 0);
			printf("written 0x%02X to lcd STAT\n", b);
			break;
		
		case 2:
			r.scy = b;
			break;
			
		case 3:
			r.scx = b;
			break;

		case 4:
			//printf("written to read-only 0xFF44!\n");
			// Actually this resets LY, if I understand the specs correctly.
			r.line = 0;
			printf("reset line counter.\n");
			break;

		case 5:
			r.lineComp = b;
			printf("written %d to lineComp\n", b);
			break;
			
		case 6: // DMA from XX00-XX9F to FE00-FE9F
			for (int i = 0; i <= 0x9F; i++) {
				memory.writeByte(memory.readByte((b << 8) + i), 0xFE00 + i);
			}
			break;
			
		case 7: // Background palette
			for (int i = 0; i < 4; i++) {
				bgpalette[i] = (b >> (i * 2)) & 3;
			}
			break;
			
		case 8: // Object palette 0
			for (int i = 0; i < 4; i++) {
				objpalette[0][i] = (b >> (i * 2)) & 3;
			}
			break;
			
		case 9: // Object palette 1
			for (int i = 0; i < 4; i++) {
				objpalette[1][i] = (b >> (i * 2)) & 3;
			}
			break;
			
		default:
			printf("TODO! write to unimplemented 0x%02X\n", addr + 0xFF40);
			break;
	}
}

inline void Graphics::setPixelColor(int x, int y, uint8_t color)
{
	uint8_t palettecols[4] = {255, 192, 96, 0};
	
	if (x >= GB_SCREEN_WIDTH || y >= GB_SCREEN_HEIGHT || x < 0 || y < 0) {
		return;
	}
	
	// rgba
	uint32_t pixel = 0xFF000000 | palettecols[color] << 16 | palettecols[color] << 8 | palettecols[color];
	screenPixels[y * GB_SCREEN_WIDTH + x] = pixel;
}

inline void Graphics::setDebugPixelColor(int x, int y, uint8_t color)
{
	uint8_t palettecols[4] = {255, 192, 96, 0};
	
	if (x >= DEBUG_WIDTH || y >= DEBUG_HEIGHT || x < 0 || y < 0) {
		return;
	}
	
	// rgba
	uint32_t pixel = 0xFF000000 | palettecols[color] << 16 | palettecols[color] << 8 | palettecols[color];
	debugPixels[y * DEBUG_WIDTH + x] = pixel;
}

void Graphics::printDebug()
{
	printf("bgtoggle=%d,spritetoggle=%d,lcdtoggle=%d,bgmap=%d,tileset=%d,scx=%d,scy=%d\n",
		(r.flags & Flag::BG) ? 1 : 0, 
		(r.flags & Flag::SPRITES) ? 1 : 0, 
		(r.flags & Flag::LCD) ? 1 : 0,
		(r.flags & Flag::TILEMAP) ? 1 : 0,
		(r.flags & Flag::TILESET) ? 1 : 0,
		r.scx, r.scy);
		
	printf("spritedata[0].x = 0x%02X.\n", spritedata[0].x);
	printf("0xC000: %02X %02X %02X %02X.\n", readByte(0xC000), readByte(0xC001), readByte(0xC002), readByte(0xC003));
	printf("0xFE00: %02X %02X %02X %02X.\n", readByte(0xFE00), readByte(0xFE01), readByte(0xFE02), readByte(0xFE03));
	/*
	int base = 0x1800;
	int i;
	
	for(i=base; i<=base+0x3FF; i++) {
		if (i%32 == 0) printf("\n");
		printf("%02X ", vram[i]);
	}
	printf("\n");
	*/
}

void Graphics::renderDebugBackground()
{
	int color;

	for (int y = 0; y < 24; ++y)
	{
		for (int x = 0; x < 16; ++x)
		{
			for (int i = 0; i < 8; ++i)
			{
				for (int j = 0; j < 8; ++j)
				{
					color = bgpalette[tileset[y*16 + x][i][j]];
					setDebugPixelColor(x * 8 + j, y * 8 + i, color);
				}
			}
		}
	}
}

void Graphics::renderScanline()
{
	uint16_t yoff, xoff, tilenr;
	uint8_t row, col, px;
	uint8_t color;
	uint8_t bgScanline[161];
	int bit, offset;

	// Background
	if (r.flags & Flag::BG) {
		// Use tilemap 1 or tilemap 0?
		yoff = (r.flags & Flag::TILEMAP) ? TILEMAP_ADDR1 : TILEMAP_ADDR0;
	
		// divide y offset by 8 (to get whole tile)
		// then multiply by 32 (= amount of tiles in a row)
		yoff += (((r.line + r.scy) & 255) >> 3) << 5;
	
		// divide x scroll by 8 (to get whole tile)
		xoff = (r.scx >> 3) & 0x1F;
	
		// row number inside our tile, we only need 3 bits
		row = (r.line + r.scy) & 0x07;
	
		// same with column number
		col = r.scx & 0x07;
	
		// tile number from bgmap
		tilenr = vram[yoff + xoff];

		// TODO: tilemap signed stuff
		//printf("TEST: signed (int8_t)tilenr = %d\n", (int8_t)tilenr);
		if (not (r.flags & Flag::TILESET) and tilenr < 128) {
			tilenr = tilenr + 256;
		}
		
		
	
		for (int i = 0; i < 160; i++) {
			bgScanline[160 - col] = tileset[tilenr][row][col];
			color = bgpalette[tileset[tilenr][row][col]];
		
			/*
			bit = 1 << (7 - col);
			offset = tilenr * 16 + row * 2;
			color = bgpalette[((vram[offset] & bit) ? 0x01 : 0x00)
								| ((vram[offset+1] & bit) ? 0x02 : 0x00)];
			*/
			
			setPixelColor(i, r.line, color);
		
			col++;
			if (col == 8) {
				col = 0;
				xoff = (xoff + 1) & 0x1F;
				tilenr = vram[yoff + xoff];

				if (not (r.flags & Flag::TILESET) and tilenr < 128) {
					tilenr = tilenr + 256;
				}
			}
		}
	}
	
	// Sprites
	if (r.flags & Flag::SPRITES) {
		uint8_t spriteHeight = (r.flags & Flag::SPRITESIZE) ? 16 : 8;

		// the upper 8x8 tile is "NN AND FEh", and the lower 8x8 tile is "NN OR 01h".

		for (int i = 0; i < 40; i++) {
			// do we need to draw the sprite?
			//printf("sprite x: %d, sprite y: %d, line: %d\n", spritedata[i].x, spritedata[i].y, r.line);
			if (spritedata[i].y <= r.line and spritedata[i].y > r.line - spriteHeight) {
				// determine row, flip y if wanted
				if (spritedata[i].flags & SpriteFlag::YFLIP) {
					row = (spriteHeight - 1) - (r.line - spritedata[i].y);
				} else {
					row = r.line - spritedata[i].y;
				}
				
				// loop through the columns
				for (int col = 0; col < 8; col++) {
					if (spritedata[i].flags & SpriteFlag::XFLIP) {
						px = spritedata[i].x + (7 - col);
					} else {
						px = spritedata[i].x + col;
					}
					
					// only draw if this pixel's on the screen
					if (px >= 0 and px < 160) {
						uint8_t spriteTile = spritedata[i].tile;
						uint8_t spriteRow = row;

						if (spriteHeight == 16) {
							if (row < 8) {
								spriteTile = spritedata[i].tile & 0xFE;
							} else {
								spriteTile = spritedata[i].tile | 0x01;
								spriteRow = row % 8;
							}
						}



						color = objpalette[(spritedata[i].flags & SpriteFlag::PALETTE) ? 1 : 0]
								[tileset[spriteTile][spriteRow][col]];

						// only draw pixel when color is not 0 and (sprite has priority or background pixel is 0)
						if (tileset[spriteTile][spriteRow][col] != 0 and
							(not (spritedata[i].flags & SpriteFlag::PRIORITY)
								or bgpalette[bgScanline[px]] == 0)) {
													  
							setPixelColor(px, r.line, color);
						}
					}
				}
			}
		}
	}
}

void Graphics::updateTile(uint8_t b, uint16_t addr)
{
	int tile = (addr >> 4) & 0x1FF;
	int row = (addr >> 1) & 0x07;
	
	//printf("updateTile! tile=%d, row=%d.\n", tile, row);
	
	for (int col = 0; col < 8; col++) {
		int bit = 1 << (7 - col);
		tileset[tile][row][col] = ((vram[addr] & bit) ? 1 : 0);
		tileset[tile][row][col] |= ((vram[addr+1] & bit) ? 2 : 0);
	}
}

void Graphics::buildSpriteData(uint8_t b, uint16_t addr)
{
	uint16_t spriteNum = addr >> 2;
	
	if (spriteNum < 40) { // Only 40 sprites
		switch (addr & 0x03) {
			case 0: // Y-coord
				spritedata[spriteNum].y = b - 16;
				//printf("Sprite #%d to Y of %d.\n", spriteNum, b-16);
				break;
				
			case 1: // X-coord
				spritedata[spriteNum].x = b - 8;
				//printf("Sprite #%d to X of %d.\n", spriteNum, b-8);
				break;
			
			case 2: // Tile
				spritedata[spriteNum].tile = b;
				break;
				
			case 3: // Flags
				spritedata[spriteNum].flags = b;
				break;
		}
	}
}

void Graphics::renderGUI()
{
	// Window positions and sizes
	auto vecSettingsPos = ImVec2(0,0);
	auto vecSettingsSize = ImVec2(200, WINDOW_HEIGHT);

	auto vecInfoPos = ImVec2(vecSettingsPos.x + vecSettingsSize.x, 0);
	auto vecInfoSize = ImVec2(GB_SCREEN_WIDTH * screenScale, 60);

	auto vecScreenPos = ImVec2(vecSettingsPos.x + vecSettingsSize.x, vecInfoSize.y);
	auto vecScreenSize = ImVec2(GB_SCREEN_WIDTH * screenScale, WINDOW_HEIGHT);

	auto vecDebugPos = ImVec2(vecScreenPos.x + vecScreenSize.x, 0);
	auto vecDebugSize = ImVec2(200, WINDOW_HEIGHT);

	// Set global style
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;


	// Info window
	ImGui::SetNextWindowPos(vecInfoPos);
	ImGui::SetNextWindowSize(vecInfoSize);
	ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize);
	ImGui::Text("%s, MBC: %s, Country: %s\nRom size: %d, Ram size: %d",
		memory.romheader->gamename,
		memory.mbcAsString().c_str(),
		memory.romheader->country ? "Other" : "Japan",
		memory.romheader->romsize,
		memory.romheader->ramsize);
	ImGui::End();


	// Settings window
	ImGui::SetNextWindowPos(vecSettingsPos);
	ImGui::SetNextWindowSize(vecSettingsSize);
	ImGui::Begin("Contols", nullptr, ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize);
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
	ImGui::SliderInt("Scale", &screenScale, 1, 8);
	ImGui::End();


	// Debug window
	ImGui::SetNextWindowPos(vecDebugPos);
	ImGui::SetNextWindowSize(vecDebugSize);
	ImGui::Begin("Debug stuffs", nullptr, ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize);

	if (ImGui::CollapsingHeader("CPU Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
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

		ImGui::Text("cycles: %d", cpu.c);
	}

	if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("1:%s, 2:%s 3:%s, 4:%s", 
			audio.ch1.isEnabled ? "on " : "off", audio.ch2.isEnabled ? "on " : "off",
			audio.ch3.isEnabled ? "on " : "off", audio.ch4.isEnabled ? "on " : "off");
	}

	if (ImGui::CollapsingHeader("Video memory", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImVec2 tex_screen_pos = ImGui::GetCursorScreenPos();
		ImGui::Image((void*)((intptr_t)debugTexture), ImVec2(DEBUG_WIDTH, DEBUG_HEIGHT),
		ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			int tilex = (int)(ImGui::GetMousePos().x - tex_screen_pos.x) / 8;
			int tiley = (int)(ImGui::GetMousePos().y - tex_screen_pos.y) / 8;
			ImGui::Image((void*)((intptr_t)debugTexture), ImVec2(80, 80),
			ImVec2(tilex*(1.0/16),tiley*(1.0/24)), ImVec2((tilex+1)*(1.0/16),(tiley+1)*(1.0/24)), ImColor(255,255,255,255), ImColor(0,0,0,0));
			ImGui::EndTooltip();
		}
	}

	ImGui::End();

	// GB Screen window (borderless, no padding)
	ImGui::SetNextWindowPos(vecScreenPos);
	ImGui::SetNextWindowSize(vecScreenSize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("GB Screen", nullptr, ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse
		);
	ImGui::Image((void*)((intptr_t)screenTexture), ImVec2(GB_SCREEN_WIDTH * screenScale, GB_SCREEN_HEIGHT * screenScale),
		ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
	ImGui::End();
	ImGui::PopStyleVar();


}

void Graphics::renderFrame()
{
	// render main window
	//SDL_UpdateTexture(screenTexture, NULL, screenPixels, GB_SCREEN_WIDTH * sizeof(uint32_t));
	//SDL_RenderClear(screenRenderer);
	//SDL_RenderCopy(screenRenderer, screenTexture, NULL, NULL);
	//SDL_RenderPresent(screenRenderer);

	int w, h;
	int display_w, display_h;
	SDL_GetWindowSize(mainWindow, &w, &h);
	SDL_GL_GetDrawableSize(mainWindow, &display_w, &display_h);

	ImGui_ImplSdlGL3_NewFrame(mainWindow);

	updateTextures();
    
	renderGUI();

	glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
	ImVec4 clear_color = ImColor(255, 255, 255, 128);
	glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui::Render();
	SDL_GL_SwapWindow(mainWindow);
}

void Graphics::step()
{
	mclock += cpu.dc;
	switch (mode) {
		case Mode::HBLANK:
			if (mclock >= 51) {
				mclock = 0;
				r.line++;

				//printf("coinInt = %d, line = %d, lineComp = %d\n", CoinInt, r.line, r.lineComp);
				if (CoinInt and r.line == r.lineComp) {
						cpu.intFlags |= CPU::Int::LCDSTAT;
				}

				
				if (r.line == 144) { // last line
					mode = Mode::VBLANK;
					cpu.intFlags |= CPU::Int::VBLANK;

					if (vBlankInt) {
						cpu.intFlags |= CPU::Int::LCDSTAT;
					}

					renderFrame();
				}
				else {
					mode = Mode::OAM;

					if (OAMInt) {
						cpu.intFlags |= CPU::Int::LCDSTAT;
					}
				}
			}
			break;
			
		case Mode::VBLANK:
			if (mclock >= 114) {
				mclock = 0;
				r.line++;
				
				if (r.line > 153) {
					mode = Mode::OAM;
					r.line = 0;

					if (OAMInt) {
						cpu.intFlags |= CPU::Int::LCDSTAT;
					}
				}
			}
			break;
			
		case Mode::OAM:
			if (mclock >= 20) {
				mclock = 0;
				mode = Mode::VRAM;
			}
			break;
			
		case Mode::VRAM:
			if (mclock >= 43) {
				mclock = 0;
				mode = Mode::HBLANK;
				renderScanline();

				if (hBlankInt) {
					cpu.intFlags |= CPU::Int::LCDSTAT;
				}
			}
	}
}
